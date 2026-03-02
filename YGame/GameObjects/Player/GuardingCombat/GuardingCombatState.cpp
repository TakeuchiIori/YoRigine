#include "GuardingCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"
#include "../Guard/PlayerGuard.h"

/// <summary>
/// コンストラクタ：ガード時のアニメーションや挙動コールバックを設定
/// </summary>
/// <param name="combat">プレイヤーの戦闘管理クラス</param>
GuardingCombatState::GuardingCombatState(PlayerCombat* combat) : combat_(combat) {
	auto* guard = combat_->GetGuard();
	auto* player = combat_->GetOwner();

	//---------------------------------------------------------------------------------------------
	// ガードステートの変更に応じてアニメーションを切り替える
	//---------------------------------------------------------------------------------------------
	guard->SetStateChangeCallback([combat, player](PlayerGuard::State from, PlayerGuard::State to) {
		// 死亡中は処理を行わない
		if (combat->GetCurrentState() == CombatState::Dead) return;

		// コンボ中など他の行動中は無視
		if (!combat->IsIdle() && from != PlayerGuard::State::Idle) return;

		auto* obj = player->GetObject3d();
		auto* movement = player->GetMovement();

		//-----------------------------------------------------------------------------------------
		// 各ステートに応じたアニメーション切り替え処理
		//-----------------------------------------------------------------------------------------
		switch (to) {
		case PlayerGuard::State::StartUp:
			obj->SetMotionSpeed(player->GetMotionSpeed(2)); // motionSpeed[2]
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

	//---------------------------------------------------------------------------------------------
	// ガード成功時（通常ガード）
	//---------------------------------------------------------------------------------------------
	guard->SetOnGuardSuccess([player]() {
		});

	//---------------------------------------------------------------------------------------------
	// パリィ成功時の処理（CC回復＋即反撃）
	//---------------------------------------------------------------------------------------------
	guard->SetOnParrySuccess([combat, player]() {
		// 死亡中は無視
		if (combat->GetCurrentState() == CombatState::Dead) return;

		// パリィ成功でCCを少量回復
		combat->GetCombo()->RecoverCC(1);
		});

	//---------------------------------------------------------------------------------------------
	// ガード失敗時の処理（被ダメージ等）
	//---------------------------------------------------------------------------------------------
	guard->SetOnGuardFail([player]() {
		// 今後ここで player->TakeDamage(◯◯); などを実装予定
		});
}

/// <summary>
/// ガード状態へ入った際の初期処理
/// </summary>
void GuardingCombatState::OnEnter() {
	// 特に初期化は不要だが、将来的にエフェクトなど追加可能
}

/// <summary>
/// ガード状態を抜けた際の処理
/// </summary>

void GuardingCombatState::OnExit() {
	auto* player = combat_->GetOwner();
	player->GetMovement()->SetCanMove(true);
	player->GetMovement()->SetCanRotate(true);
}

/// <summary>
/// 毎フレームの更新処理
/// </summary>
/// <param name="deltaTime">経過時間</param>
void GuardingCombatState::Update([[maybe_unused]] float deltaTime) {
	//---------------------------------------------------------------------------------------------
	// ガードが Idle 状態に戻ったら戦闘ステートも Idle に復帰
	//---------------------------------------------------------------------------------------------
	if (combat_->GetGuard()->GetState() == PlayerGuard::State::Idle) {
		machine_->ChangeState(CombatState::Idle);
	}
}
