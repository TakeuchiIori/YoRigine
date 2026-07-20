#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

/// <summary>
/// 攻撃状態
/// </summary>
class BattleRushAttackState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;
	void Update(BattleEnemy& enemy, float dt) override;
	void Exit(BattleEnemy& enemy) override;

	bool IsAttacking() const override { return true; }
	bool IsContactDamageActive() const override { return isContactDamageActive_; }

private:
	Vector3 attackDir_{ 0,0,0 };
	Vector3 anticipationStartPos_{ 0,0,0 };
	bool dirLocked_ = false;
	bool isContactDamageActive_ = false;
};
