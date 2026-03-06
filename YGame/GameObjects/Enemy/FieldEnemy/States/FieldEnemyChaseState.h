#pragma once
#include "../../IEnemyState.h"

class FieldEnemy;

/// <summary>
/// 追跡状態
/// 見失った場合は Patrol に直接戻らず、SearchState を挟む
/// </summary>
class FieldEnemyChaseState : public IEnemyState<FieldEnemy> {
public:
	void Enter(FieldEnemy& enemy) override;
	void Update(FieldEnemy& enemy, float dt) override;
	void Exit(FieldEnemy& enemy) override;

private:
	void ChasePlayer(FieldEnemy& enemy, float dt);
	bool ShouldGiveUpChase(const FieldEnemy& enemy) const;
};