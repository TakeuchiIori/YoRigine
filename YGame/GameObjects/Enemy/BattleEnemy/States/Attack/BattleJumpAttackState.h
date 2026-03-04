#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"
#include "../BattleIdleState.h"

/// <summary>
/// 跳躍攻撃状態（ジャンプして叩きつける）
/// </summary>
class BattleJumpAttackState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;

	void Update(BattleEnemy& enemy, float dt) override;

	void Exit(BattleEnemy& enemy) override;

private:
	Vector3 startPos_{ 0, 0, 0 };
	Vector3 targetPos_{ 0, 0, 0 };
	float startY_ = 0.0f;
};