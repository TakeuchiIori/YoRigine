#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"

#include <random>

/// <summary>
/// 待機状態
/// </summary>
class BattleIdleState : public IEnemyState<BattleEnemy> {
public:
	static Vector3 GetRandomOffset(float radius = 3.0f);

	void Enter(BattleEnemy& enemy) override;
	void Update(BattleEnemy& enemy, float dt)override;
	void Exit(BattleEnemy& enemy) override;
};
