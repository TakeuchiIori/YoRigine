#pragma once
#include "../../IEnemyState.h"
#include "Vector3.h"

class FieldEnemy;

/// <summary>
/// パトロール状態
/// プレイヤーを発見した場合は Chase に直接遷移せず、AlertState を挟む
/// </summary>
class FieldEnemyPatrolState : public IEnemyState<FieldEnemy> {
public:
	void Enter(FieldEnemy& enemy) override;
	void Update(FieldEnemy& enemy, float dt) override;
	void Exit(FieldEnemy& enemy) override;

private:
	void GenerateNewPatrolTarget(FieldEnemy& enemy);
	bool HasReachedTarget(const FieldEnemy& enemy) const;
	void MoveTowardsTarget(FieldEnemy& enemy, float dt);
	void CheckForPlayer(FieldEnemy& enemy);
};