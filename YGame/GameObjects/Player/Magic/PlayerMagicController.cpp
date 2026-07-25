#include "PlayerMagicController.h"

#include "MagicActionDatabase.h"
#include "../Player.h"
#include "../Combat/PlayerCombat.h"
#include "../Movement/PlayerMovement.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

PlayerMagicController::PlayerMagicController(Player* owner)
	: owner_(owner)
{
	MagicActionDatabase::LoadFromFile("Resources/Json/Magic/MagicActions.json");
}

void PlayerMagicController::Update(float deltaTime)
{
	runner_.Update(deltaTime);

	// 詠唱の全身アニメ（Once）が終わったら、Movement ステートに応じたアニメ
	// （走り／歩き／待機）へ戻す。これをしないと走行中でも最終フレームで固まる。
	if (waitingCastAnimEnd_ && owner_) {
		PlayerCombat* combat = owner_->GetCombat();
		if (combat && !combat->IsIdle()) {
			// 被弾・スタン等で戦闘ステートが割り込んだ場合は、そのステート側の
			// 復帰（OnExit の SyncAnimationToCurrentState）に任せる。
			waitingCastAnimEnd_ = false;
		}
		else if (!runner_.IsRunning()) {
			YoRigine::Object3d* obj = owner_->GetObject3d();
			YoRigine::Model* model = obj ? obj->GetModel() : nullptr;
			auto* motion = model ? model->GetMotionSystem() : nullptr;
			if (motion && motion->IsFinished()) {
				owner_->GetMovement()->SyncAnimationToCurrentState();
				waitingCastAnimEnd_ = false;
			}
		}
	}

	for (int i = 0; i < 3; ++i) {
		if (comboTimers_[i] <= 0.0f) continue;
		comboTimers_[i] = std::max(0.0f, comboTimers_[i] - deltaTime);
		if (comboTimers_[i] <= 0.0f) {
			comboIndices_[i] = 0;
		}
	}

	for (auto& attack : activeAttacks_) {
		if (attack) attack->Update(deltaTime);
	}
	activeAttacks_.erase(
		std::remove_if(activeAttacks_.begin(), activeAttacks_.end(),
			[](const std::unique_ptr<MagicAttackInstance>& attack) {
				return !attack || !attack->IsAlive();
			}),
		activeAttacks_.end());
}

void PlayerMagicController::Reset()
{
	runner_.Reset();
	activeAttacks_.clear();
	hasPendingChargeAction_ = false;
	waitingCastAnimEnd_ = false;
	for (bool& held : previousHeld_) {
		held = false;
	}
	for (int& index : comboIndices_) {
		index = 0;
	}
	for (float& timer : comboTimers_) {
		timer = 0.0f;
	}
}

bool PlayerMagicController::TryCast(PlayerMagicSlot slot)
{
	const int index = SlotIndex(slot);
	const MagicActionData* action = MagicActionDatabase::FindBySlotAt(slot, comboIndices_[index]);
	if (!action) return false;
	if (!BeginCast(*action)) return false;
	AdvanceChain(slot, *action);
	return true;
}

void PlayerMagicController::HandleSlotInput(PlayerMagicSlot slot, bool triggered, bool held)
{
	const int index = SlotIndex(slot);
	const bool released = previousHeld_[index] && !held;
	previousHeld_[index] = held;

	if (triggered) {
		TryCast(slot);
	}
	if (released) {
		// 溜め成立時のみ実弾を出す。不発でも保留は必ず畳み、次のチャージへ持ち越さない。
		if (runner_.Release() && hasPendingChargeAction_) {
			// チャージ量に応じて威力と判定半径を強化する（満タンでダメージ2倍・半径1.5倍）。
			// scaleCurve が判定半径に掛かるのと同じ流儀で、見た目(travelVfx/impactVfx)は
			// アセット側、ゲーム値はここでスケールと役割を分ける。
			MagicActionData action = pendingChargeAction_;
			const float maxCharge = std::max(0.01f, action.maxChargeTime);
			const float chargeRatio =
				std::clamp(runner_.GetChargeTime() / maxCharge, 0.0f, 1.0f);
			action.damage *= 1.0f + chargeRatio;
			action.hitRadius *= 1.0f + 0.5f * chargeRatio;
			SpawnAttackInstance(action);
		}
		hasPendingChargeAction_ = false;
	}
}

void PlayerMagicController::DrawCollision()
{
	for (auto& attack : activeAttacks_) {
		if (attack) attack->DrawCollision();
	}
}

bool PlayerMagicController::CanCast() const
{
	if (!owner_ || !owner_->GetCombat()) return false;

	// 魔法は剣とは別系統だが、被弾・スタン・死亡などの全身ロックは戦闘ステートを正とする。
	// ここでは状態を読むだけに留め、PlayerCombat に魔法固有の分岐を増やさない。
	return owner_->GetCombat()->CanAct()
		&& owner_->GetCombat()->IsIdle()
		&& !runner_.IsRunning()
		&& runner_.GetCooldown() <= 0.0f;
}

bool PlayerMagicController::BeginCast(const MagicActionData& action)
{
	if (!CanCast()) return false;

	runner_.Start(action, owner_);
	if (action.inputMode == MagicInputMode::ChargeRelease) {
		pendingChargeAction_ = action;
		hasPendingChargeAction_ = true;
	}
	else {
		SpawnAttackInstance(action);
	}

	if (owner_ && owner_->GetObject3d() && !action.animationName.empty()) {
		owner_->GetObject3d()->SetMotionSpeed(1.0f);
		owner_->GetObject3d()->SetChangeMotion("Player.gltf", MotionPlayMode::Once, action.animationName);
		// この Once アニメが終わったら Movement 側のアニメ（走り等）へ戻す。
		waitingCastAnimEnd_ = true;
	}
	return true;
}

void PlayerMagicController::SpawnAttackInstance(const MagicActionData& action)
{
	if (!owner_) return;
	activeAttacks_.push_back(MagicAttackFactory::Create(action, owner_));
}

void PlayerMagicController::AdvanceChain(PlayerMagicSlot slot, const MagicActionData& action)
{
	const int index = SlotIndex(slot);
	const int count = MagicActionDatabase::CountBySlot(slot);
	if (count <= 1) {
		comboIndices_[index] = 0;
		comboTimers_[index] = 0.0f;
		return;
	}

	// 魔法の連携は「同じスロット内の並び順」を段数として扱う。
	// 個別クラスを増やす前にデータ順だけで試せるようにし、剣コンボの状態機械とは分離する。
	comboIndices_[index] = (comboIndices_[index] + 1) % count;
	comboTimers_[index] = std::max(0.0f, action.chainResetTime);
}

void PlayerMagicController::ResetChain(PlayerMagicSlot slot)
{
	const int index = SlotIndex(slot);
	comboIndices_[index] = 0;
	comboTimers_[index] = 0.0f;
}

int PlayerMagicController::SlotIndex(PlayerMagicSlot slot) const
{
	switch (slot) {
	case PlayerMagicSlot::Primary:
		return 0;
	case PlayerMagicSlot::Secondary:
		return 1;
	case PlayerMagicSlot::Utility:
		return 2;
	default:
		return 0;
	}
}

#ifdef USE_IMGUI
void PlayerMagicController::ShowDebugImGui()
{
	if (ImGui::BeginTabBar("MagicDebugSystem")) {
		if (ImGui::BeginTabItem("魔法")) {
			ImGui::Text("実行中の魔法: %s", runner_.GetCurrentActionName().c_str());
			ImGui::Text("クールダウン: %.2f", runner_.GetCooldown());
			ImGui::Text("主魔法(A) 段数: %d / %.2f", comboIndices_[0] + 1, comboTimers_[0]);
			ImGui::Text("副魔法(B) 段数: %d / %.2f", comboIndices_[1] + 1, comboTimers_[1]);
			ImGui::Text("補助魔法(X) 段数: %d / %.2f", comboIndices_[2] + 1, comboTimers_[2]);
			if (ImGui::Button("A 主魔法")) { TryCast(PlayerMagicSlot::Primary); } ImGui::SameLine();
			if (ImGui::Button("B 副魔法")) { TryCast(PlayerMagicSlot::Secondary); } ImGui::SameLine();
			if (ImGui::Button("X 補助魔法")) { TryCast(PlayerMagicSlot::Utility); }
			if (ImGui::Button("魔法コンボリセット")) {
				ResetChain(PlayerMagicSlot::Primary);
				ResetChain(PlayerMagicSlot::Secondary);
				ResetChain(PlayerMagicSlot::Utility);
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void PlayerMagicController::DrawEditorWindow()
{
	editor_.Draw();
}
#endif
