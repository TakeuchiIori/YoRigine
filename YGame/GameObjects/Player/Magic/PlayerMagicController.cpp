#include "PlayerMagicController.h"

#include "MagicActionDatabase.h"
#include "../Player.h"
#include "../Combat/PlayerCombat.h"

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
}

void PlayerMagicController::Reset()
{
	runner_.Reset();
	for (bool& held : previousHeld_) {
		held = false;
	}
}

bool PlayerMagicController::TryCast(PlayerMagicSlot slot)
{
	const MagicActionData* action = MagicActionDatabase::FindBySlot(slot);
	if (!action) return false;
	return BeginCast(*action);
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
		runner_.Release();
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
	return true;
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
			if (ImGui::Button("主魔法")) { TryCast(PlayerMagicSlot::Primary); } ImGui::SameLine();
			if (ImGui::Button("副魔法")) { TryCast(PlayerMagicSlot::Secondary); } ImGui::SameLine();
			if (ImGui::Button("補助魔法")) { TryCast(PlayerMagicSlot::Utility); }
			ImGui::Separator();
			editor_.Draw();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}
#endif
