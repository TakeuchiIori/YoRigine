#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"

/// <summary>
/// 死亡状態：縮小と同時にディゾルブで消えていく
/// </summary>
class BattleDeadState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;
	void Update(BattleEnemy& enemy, float dt) override;
	void Exit(BattleEnemy& enemy) override;

private:
	// 死亡演出経過時間
	float deathTimer_ = 0.0f;
};
