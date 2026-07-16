#include "GuardingCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"
#include "../Guard/PlayerGuard.h"

// ============================================================
// コンストラクタ
// ガード時のアニメーション切り替えや成功/失敗時のコールバックを設定する
// ============================================================
GuardingCombatState::GuardingCombatState(PlayerCombat* combat) : combat_(combat) {
	auto* guard = combat_->GetGuard();
	auto* player = combat_->GetOwner();

	// ------------------------------------------------------------
	// ガードステート変更に応じたアニメーションと移動の制御
	// ------------------------------------------------------------
	guard->SetStateChangeCallback([combat, player](PlayerGuard::State from, PlayerGuard::State to) {
		(void)from;
		if (combat->GetCurrentState() == CombatState::Dead) return;

		auto* obj = player->GetObject3d();
		auto* movement = player->GetMovement();

		switch (to) {
		case PlayerGuard::State::StartUp:
			obj->SetMotionSpeed(player->GetMotionSpeed(2));
			obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, "Block1");
			movement->SetCanMove(false);
			movement->SetCanRotate(false);
			break;

		case PlayerGuard::State::Active:
			obj->SetMotionSpeed(player->GetMotionSpeed(2));
			obj->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Block_Idle");
			break;

		case PlayerGuard::State::Recovery:
			obj->SetMotionSpeed(player->GetMotionSpeed(2));
			obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, "Block2");
			break;

		case PlayerGuard::State::Idle:
			break;
		}
		});

	// ------------------------------------------------------------
	// ガード・パリィ成功/失敗時のイベントハンドラ
	// ------------------------------------------------------------
	guard->SetOnGuardSuccess([player]() {
		});

	guard->SetOnParrySuccess([combat, player]() {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->GetCombo()->RecoverCC(1);
		});

	guard->SetOnGuardFail([player]() {
		});
}

// ============================================================
// ステート開始処理
// ============================================================
void GuardingCombatState::OnEnter() {
}

// ============================================================
// ステート終了処理
// ガード解除後、移動と回転を再び可能にする
// ============================================================
void GuardingCombatState::OnExit() {
	auto* player = combat_->GetOwner();
	player->GetMovement()->SetCanMove(true);
	player->GetMovement()->SetCanRotate(true);

	// Movementステートに現在の状態に応じたアニメーション再生を委譲
	player->GetMovement()->SyncAnimationToCurrentState();
}

// ============================================================
// 更新処理
// ガードが完全に解除(Idle)されたら、戦闘ステート全体を待機に戻す
// ============================================================
void GuardingCombatState::Update([[maybe_unused]] float deltaTime) {
	if (combat_->GetGuard()->GetState() == PlayerGuard::State::Idle) {
		machine_->ChangeState(CombatState::Idle);
	}
}