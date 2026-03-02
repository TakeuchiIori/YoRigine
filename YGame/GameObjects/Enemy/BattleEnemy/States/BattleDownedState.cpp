#include "BattleDownedState.h"
#include "BattleIdleState.h"

/// <summary>
/// ダウン状態開始時の処理
/// </summary>
void BattleDownedState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1.0f, 1.0f, 0.0f, 1.0f });
	enemy.IsDamageBlinking() = true;
	enemy.GetWT().anchorPoint_ = Vector3{ 0.0f, -1.0f, 0.0f };
}

/// <summary>
/// ダウン中の更新処理（揺れ演出）
/// </summary>
void BattleDownedState::Update(BattleEnemy& enemy, [[maybe_unused]] float dt) {
	float t = enemy.GetStateTimer();
	float angle = t * speed_;
	float rotX = std::sin(angle) * tilt_;
	float rotZ = std::cos(angle) * tilt_;

	enemy.GetRotationX() = rotX;
	enemy.GetRotationZ() = rotZ;

	// 一定時間経過で立ち上がり
	if (t > standUpTime_) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

/// <summary>
/// ダウン終了時の処理
/// </summary>
void BattleDownedState::Exit(BattleEnemy& enemy) {
	enemy.SetCanAct(true);
	enemy.IsDamageBlinking() = false;
	enemy.SetColor({ 1, 1, 1, 1 });
	enemy.GetWT().anchorPoint_ = Vector3{ 0.0f, 0.0f, 0.0f };
	enemy.GetRotationX() = 0.0f;
	enemy.GetRotationZ() = 0.0f;
}
