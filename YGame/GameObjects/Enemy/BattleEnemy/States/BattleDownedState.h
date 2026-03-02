#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"
#include <random>

/// <summary>
/// ダウン状態
/// </summary>
class BattleDownedState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;
	void Update(BattleEnemy& enemy, float dt) override;
	void Exit(BattleEnemy& enemy) override;

private:

	// ダウン時間
	float duration_ = 1.0f;
	// 回転速度
	float speed_ = 5.0f;
	// 傾きの強さ
	float tilt_ = 0.3f;
	// 立ち上がりの時間
	float standUpTime_ = 3.5f;
};

