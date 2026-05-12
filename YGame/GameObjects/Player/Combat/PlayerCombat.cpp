#include "PlayerCombat.h"

// App
#include "../Player.h"

// State
#include "../IdleCombat/IdleCombatState.h"
#include "../AttackingCombat/AttackingCombatState.h"
#include "../GuardingCombat/GuardingCombatState.h"
#include "../DodgingCombat/DodgingCombatState.h"
#include "../StunnedCombat/StunnedCombatState.h"
#include "../DeadCombat/DeadCombatState.h"
#include "../HitCombat/HitCombatState.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

// ============================================================
// コンストラクタ
// 戦闘システムの初期化およびステートマシンのセットアップを行う
// ============================================================
PlayerCombat::PlayerCombat(Player* owner) : owner_(owner) {
	// ------------------------------------------------------------
	// システムの初期化
	// ------------------------------------------------------------
	combo_ = std::make_unique<PlayerCombo>(owner);
	guard_ = std::make_unique<PlayerGuard>(owner);

	// guardEmitter_ = std::make_unique<ParticleEmitter>("PlayerParticle", wt_.translate_, 5);
	// parryEmitter_ = std::make_unique<ParticleEmitter>("PlayerHitParticle", wt_.translate_, 5);

	// ------------------------------------------------------------
	// ステートマシンの初期化
	// ------------------------------------------------------------
	InitializeStateMachine();
}

// ============================================================
// ステートマシンの初期化
// 各戦闘ステートのインスタンスを登録する
// ============================================================
void PlayerCombat::InitializeStateMachine() {
	// ------------------------------------------------------------
	// 各状態を登録
	// ------------------------------------------------------------
	stateMachine_.RegisterState<IdleCombatState>(CombatState::Idle, this);
	stateMachine_.RegisterState<AttackingCombatState>(CombatState::Attacking, this);
	stateMachine_.RegisterState<GuardingCombatState>(CombatState::Guarding, this);
	stateMachine_.RegisterState<DodgingCombatState>(CombatState::Dodging, this);
	stateMachine_.RegisterState<StunnedCombatState>(CombatState::Stunned, this);
	stateMachine_.RegisterState<DeadCombatState>(CombatState::Dead, this);
	stateMachine_.RegisterState<HitCombatState>(CombatState::Hit, this);

	// 初期状態とオーナーを設定
	stateMachine_.SetInitialState(CombatState::Idle);
	stateMachine_.SetOwner(this);
}

// ============================================================
// 毎フレームの更新処理
// 死亡判定と各戦闘機能の更新を呼び出す
// ============================================================
void PlayerCombat::Update(float deltaTime) {
	// ------------------------------------------------------------
	// 死亡判定と遷移
	// ------------------------------------------------------------
	if (owner_->GetHP() <= 0 && GetCurrentState() != CombatState::Dead) {
		ChangeState(CombatState::Dead);
		return;
	}

	// ------------------------------------------------------------
	// ステートマシンおよび下層システムの更新
	// ------------------------------------------------------------
	stateMachine_.Update(deltaTime);
	combo_->Update(deltaTime);
	guard_->Update(deltaTime);
}

// ============================================================
// 戦闘状態のリセット
// 全てを待機状態に戻す
// ============================================================
void PlayerCombat::Reset() {
	combo_->ResetCombo();
	guard_->Reset();
	stateMachine_.ChangeState(CombatState::Idle);
}

// ============================================================
// 攻撃実行の試行
// ============================================================
bool PlayerCombat::TryAttack(AttackType type) {
	if (!CanAct()) return false;

	if (combo_->TryAttack(type)) {
		if (GetCurrentState() == CombatState::Idle) {
			ChangeState(CombatState::Attacking);
		}
		return true;
	}

	return false;
}

// ============================================================
// 回避実行の試行
// ============================================================
bool PlayerCombat::TryDodge() {
	if (!CanAct()) return false;

	ChangeState(CombatState::Dodging);
	combo_->OnDodgeSuccess();
	return true;
}

// ============================================================
// ガード実行の試行
// ============================================================
bool PlayerCombat::TryGuard() {
	if (!CanAct()) return false;

	if (guard_->StartGuard()) {
		ChangeState(CombatState::Guarding);
		return true;
	}
	return false;
}

// ============================================================
// 特殊攻撃実行の試行（未実装）
// ============================================================
bool PlayerCombat::TrySpecial() {
	if (!CanAct()) return false;
	return false;
}

// ============================================================
// コンボのアクションキャンセル
// ============================================================
bool PlayerCombat::TryCancel() {
	if (combo_->GetCurrentAttack() && combo_->GetCurrentAttack()->canCancel) {
		combo_->CancelCombo();
		ChangeState(CombatState::Idle);
		return true;
	}
	return false;
}

// ============================================================
// 各種状態確認
// ============================================================
bool PlayerCombat::IsIdle() const { return GetCurrentState() == CombatState::Idle; }
bool PlayerCombat::IsAttacking() const { return GetCurrentState() == CombatState::Attacking; }
bool PlayerCombat::IsDodging() const { return GetCurrentState() == CombatState::Dodging; }
bool PlayerCombat::IsStunned() const { return GetCurrentState() == CombatState::Stunned; }
bool PlayerCombat::IsDead() const { return GetCurrentState() == CombatState::Dead; }
bool PlayerCombat::IsGuarding() const { return GetCurrentState() == CombatState::Guarding; }
bool PlayerCombat::IsHit() const { return GetCurrentState() == CombatState::Hit; }

bool PlayerCombat::IsFullBodyAction() const {
	return IsGuarding() || IsHit() || IsStunned() || IsDead();
}

bool PlayerCombat::CanMove() const {
	return !IsStunned() && !IsDead() && !IsHit();
}

bool PlayerCombat::CanAct() const {
	return !IsStunned();
}

// ============================================================
// デバッグ表示（ImGui）
// 戦闘ステートやコンボ情報の確認用
// ============================================================
void PlayerCombat::ShowDebugImGui() {
#ifdef USE_IMGUI
	if (ImGui::BeginTabBar("CombatDebugSystem"))
	{
		// ------------------------------------------------------------
		// 戦闘ステート情報
		// ------------------------------------------------------------
		if (ImGui::BeginTabItem("状態・行動"))
		{
			const char* stateNames[] = { "Idle", "Attacking", "Guarding", "Dodging", "Stunned","Dead","Hit" };
			ImGui::Text("現在の状態: %s", stateNames[static_cast<int>(GetCurrentState())]);
			ImGui::Text("以前の状態: %s", stateNames[static_cast<int>(GetPreviousState())]);
			ImGui::Separator();

			ImGui::Text("現在のCC: %d / %d", GetCurrentCC(), GetMaxCC());
			ImGui::ProgressBar(static_cast<float>(GetCurrentCC()) / GetMaxCC());

			ImGui::EndTabItem();
		}

		// ------------------------------------------------------------
		// コンボや攻撃の情報
		// ------------------------------------------------------------
		if (ImGui::BeginTabItem("コンボ情報"))
		{
			ImGui::Text("コンボ数: %d", GetComboCount());
			ImGui::Text("倍率: x%.2f", GetComboDamageMultiplier());
			ImGui::Text("状態: %s", combo_->GetStateString(GetComboState()));

			ImGui::Separator();
			ImGui::Text("操作テスト");
			if (ImGui::Button("A攻撃")) { TryAttack(AttackType::A_Arte); } ImGui::SameLine();
			if (ImGui::Button("B攻撃")) { TryAttack(AttackType::B_Arte); } ImGui::SameLine();

			if (ImGui::Button("ガード")) { TryGuard(); } ImGui::SameLine();
			if (ImGui::Button("キャンセル")) { TryCancel(); }

			ImGui::EndTabItem();
		}

		// ------------------------------------------------------------
		// コンボ・ガードシステムの詳細
		// ------------------------------------------------------------
		if (ImGui::BeginTabItem("システム詳細"))
		{
			combo_->ShowDebugImGui();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ガード詳細"))
		{
			guard_->ShowDebugImGui();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
#endif
}