#include "AttackingCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"

// ============================================================
// コンストラクタ
// コンボシステムの各種コールバックを設定する
// ============================================================
AttackingCombatState::AttackingCombatState(PlayerCombat* combat) : combat_(combat) {
	auto* combo = combat_->GetCombo();
	auto* player = combat_->GetOwner();

	// ------------------------------------------------------------
	// 攻撃開始時のコールバック設定
	// ------------------------------------------------------------
	combo->SetAttackStartCallback([combat, player](const AttackData& attack) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		auto* movement = player->GetMovement();
		movement->SetCanMove(false);
		movement->SetCanRotate(false);
		movement->ForceStop();

		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(attack.motionSpeed);
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);

		player->GetSword()->PlayTrail();
		combat->NotifyAction("コンボ開始: " + attack.animationName);
		});

	// ------------------------------------------------------------
	// コンボ継続時のコールバック設定
	// ------------------------------------------------------------
	combo->SetAttackContinueCallback([combat, player](const AttackData& attack) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		auto* movement = player->GetMovement();
		movement->SetCanMove(false);
		movement->SetCanRotate(false);

		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(attack.motionSpeed);
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);

		player->GetSword()->PlayTrail();
		combat->NotifyAction("コンボ継続: " + attack.animationName);
		});

	// ------------------------------------------------------------
	// コンボ終了時のコールバック設定
	// ------------------------------------------------------------
	combo->SetComboEndCallback([combat, player]([[maybe_unused]] int finalCount) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		player->GetMovement()->SetCanMove(true);
		player->GetMovement()->SetCanRotate(true);

		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(player->GetMotionSpeed(0));

		player->GetSword()->StopTrail();

		combat->NotifyAction("コンボ終了");
		combat->ChangeState(CombatState::Idle);
		});

	// ------------------------------------------------------------
	// コンボリセット時のコールバック設定
	// ------------------------------------------------------------
	combo->SetComboResetCallback([combat]() {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->NotifyAction("コンボリセット");
		});

	// ------------------------------------------------------------
	// CC変化時のコールバック設定
	// ------------------------------------------------------------
	combo->SetCCChangeCallback([]([[maybe_unused]] int oldCC, [[maybe_unused]] int newCC) {
		});

	// ------------------------------------------------------------
	// 剣コライダー制御のコールバック
	// ------------------------------------------------------------
	combo->SetSwordColliderCallback([player](bool isActive) {
		player->GetSword()->SetEnableCollider(isActive);
		});
}

// ============================================================
// ステート開始処理
// ============================================================
void AttackingCombatState::OnEnter() {
	auto* player = combat_->GetOwner();
	auto* movement = player->GetMovement();

	movement->SetCanMove(false);
	movement->SetCanRotate(false);
	movement->ForceStop();
}

// ============================================================
// ステート終了処理
// ============================================================
void AttackingCombatState::OnExit() {
	auto* player = combat_->GetOwner();
	auto* movement = player->GetMovement();

	movement->SetCanMove(true);
	movement->SetCanRotate(true);

	player->GetSword()->StopTrail();
}

// ============================================================
// 更新処理
// ============================================================
void AttackingCombatState::Update([[maybe_unused]] float deltaTime) {

}