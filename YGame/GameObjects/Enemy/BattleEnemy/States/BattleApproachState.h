#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"
#include <random>

/// <summary>
/// 接近状態
/// </summary>
class BattleApproachState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override { enemy.SetHasTargetPosition(true); }
	void Update(BattleEnemy& enemy, float dt) override;
	void Exit(BattleEnemy& enemy) override { enemy.SetHasTargetPosition(false); }
};

