#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"
#include "../BattleIdleState.h"

/// <summary>
/// 回転攻撃状態（くるくる回って薙ぎ払う）
/// </summary>
class BattleSpinAttackState : public IEnemyState<BattleEnemy> {
public:
	void Enter(BattleEnemy& enemy) override;

	void Update(BattleEnemy& enemy, float dt) override;

	void Exit(BattleEnemy& enemy) override;

	bool IsAttacking() const override { return true; }
	bool IsContactDamageActive() const override { return isContactDamageActive_; }

private:
	float startRotation_ = 0.0f;
	float targetTwistRotation_ = 0.0f;
	bool isContactDamageActive_ = false;
};
