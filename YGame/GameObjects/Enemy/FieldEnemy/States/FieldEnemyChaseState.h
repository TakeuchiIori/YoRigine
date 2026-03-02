#pragma once
#include "../../IEnemyState.h"

class FieldEnemy;

/// <summary>
/// 接近状態
/// </summary>
class FieldEnemyChaseState : public IEnemyState<FieldEnemy> {
public:
	void Enter(FieldEnemy& enemy) override;
	void Update(FieldEnemy& enemy, float dt) override;
	void Exit(FieldEnemy& enemy) override;

private:
	void ChasePlayer(FieldEnemy& enemy, float dt);
	bool ShouldReturnToPatrol(const FieldEnemy& enemy) const;
	void FacePlayer(FieldEnemy& enemy);
};