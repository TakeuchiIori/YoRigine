#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"
#include <random>

/// <summary>
/// 死亡状態
/// </summary>
class BattleDeadState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;
	void Update(BattleEnemy& enemy, float dt) override;
	void Exit(BattleEnemy& enemy) override;
};

