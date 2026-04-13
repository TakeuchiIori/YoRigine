#include "AttackingCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"

/// <summary>
/// 攻撃状態コンストラクタ  
/// コンボシステムの各種コールバックを設定する
/// </summary>
/// <param name="combat">プレイヤーのコンバットコンポーネント</param>
AttackingCombatState::AttackingCombatState(PlayerCombat* combat) : combat_(combat) {
	auto* combo = combat_->GetCombo();
	auto* player = combat_->GetOwner();

	// ---------------------------------------------------------------------------------------------
	// 攻撃開始時のコールバック設定
	// ---------------------------------------------------------------------------------------------
	combo->SetAttackStartCallback([combat, player](const AttackData& attack) {
		// 死亡状態では無効
		if (combat->GetCurrentState() == CombatState::Dead) return;

		auto* movement = player->GetMovement();
		movement->SetCanMove(false);
		movement->SetCanRotate(false);

		// 即座に停止させる
		movement->ForceStop();  

		// 攻撃アニメーションを再生
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(attack.motionSpeed);
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);

		// アニメーション再生と同時に剣のトレイルエフェクトも再生開始
		player->GetSword()->PlayTrail();
		// 通知ログ
		combat->NotifyAction("コンボ開始: " + attack.animationName);
		});

	// ---------------------------------------------------------------------------------------------
	// コンボ継続時のコールバック設定
	// ---------------------------------------------------------------------------------------------
	combo->SetAttackContinueCallback([combat, player](const AttackData& attack) {
		// 死亡状態では無効
		if (combat->GetCurrentState() == CombatState::Dead) return;

		auto* movement = player->GetMovement();
		movement->SetCanMove(false);
		movement->SetCanRotate(false);

		// 攻撃アニメーション再生
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(attack.motionSpeed);
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);

		// トレイルをリセットして再生開始
		player->GetSword()->PlayTrail();
		// 通知ログ
		combat->NotifyAction("コンボ継続: " + attack.animationName);
		});

	// ---------------------------------------------------------------------------------------------
	// コンボ終了時のコールバック設定
	// ---------------------------------------------------------------------------------------------
	combo->SetComboEndCallback([combat, player]([[maybe_unused]] int finalCount) {
		// 死亡状態では無効
		if (combat->GetCurrentState() == CombatState::Dead) return;

		// 攻撃終了後は移動・回転を有効化
		player->GetMovement()->SetCanMove(true);
		player->GetMovement()->SetCanRotate(true);

		// 現在の移動状態に応じてアニメーションを再設定
		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(player->GetMotionSpeed(0));

		// トレイルエフェクトを停止
		player->GetSword()->StopTrail();

		// 通知ログ
		combat->NotifyAction("コンボ終了");
		// 攻撃終了後にIdle状態へ戻す
		combat->ChangeState(CombatState::Idle);
		});

	// ---------------------------------------------------------------------------------------------
	// コンボリセット時のコールバック設定
	// ---------------------------------------------------------------------------------------------
	combo->SetComboResetCallback([combat]() {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->NotifyAction("コンボリセット");
		});

	// ---------------------------------------------------------------------------------------------
	// CC変化時のコールバック設定
	// ---------------------------------------------------------------------------------------------
	combo->SetCCChangeCallback([]([[maybe_unused]] int oldCC, [[maybe_unused]] int newCC) {
		// CC（Chain Capacity）の変化通知など
		});

	// ---------------------------------------------------------------------------------------------
	// 剣コライダーの ON/OFF をフレーム精度で制御
	// ---------------------------------------------------------------------------------------------
	combo->SetSwordColliderCallback([player](bool isActive) {
		player->GetSword()->SetEnableCollider(isActive);
		});
}

/// <summary>
/// 攻撃状態に入ったときに呼ばれる
/// </summary>
void AttackingCombatState::OnEnter() {
	// 攻撃状態突入時の初期処理（必要ならここに追加）
	auto* player = combat_->GetOwner();
	auto* movement = player->GetMovement();

	movement->SetCanMove(false);
	movement->SetCanRotate(false);
	movement->ForceStop();
}

/// <summary>
/// 攻撃状態を抜けるときに呼ばれる
/// </summary>
void AttackingCombatState::OnExit() {
	// 攻撃状態終了時の後処理（必要ならここに追加）
	auto* player = combat_->GetOwner();
	auto* movement = player->GetMovement();

	movement->SetCanMove(true);
	movement->SetCanRotate(true);

	// 攻撃終了後はエフェクトも停止
	player->GetSword()->StopTrail();
}

/// <summary>
/// 攻撃状態の毎フレーム更新処理
/// </summary>
/// <param name="deltaTime">フレームの経過時間</param>
void AttackingCombatState::Update([[maybe_unused]] float deltaTime) {

}
