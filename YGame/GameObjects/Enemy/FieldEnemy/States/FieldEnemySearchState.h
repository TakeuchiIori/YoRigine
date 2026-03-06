#pragma once
#include "../../IEnemyState.h"

class FieldEnemy;

/// <summary>
/// 索敵状態（プレイヤーを見失った後の「？」演出）
/// 見失った地点でその場に止まり、左右を見渡してから Patrol へ戻る
/// スウィープ角度・時間は FieldEnemyData で調整可能
/// </summary>
class FieldEnemySearchState : public IEnemyState<FieldEnemy> {
public:
	void Enter(FieldEnemy& enemy) override;
	void Update(FieldEnemy& enemy, float dt) override;
	void Exit(FieldEnemy& enemy) override;

private:
	// 索敵開始時の向き（ラジアン）
	float entryAngle_ = 0.0f;

	// 全体タイマー
	float timer_ = 0.0f;

	// スウィープ 1 往復あたりの時間
	float sweepPeriod_ = 0.0f;
};