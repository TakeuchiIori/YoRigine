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
	void RequestNewPath(FieldEnemy& enemy);
	bool ShouldGiveUpChase(const FieldEnemy& enemy) const;

	float chaseTimer_ = 0.0f;
	float losLostTimer_ = 0.0f;            // ← 追加
	static constexpr float kMinChaseTime = 1.5f;
	static constexpr float kLosLostThreshold = 1.0f; // 1.5秒視線断絶で諦める
};