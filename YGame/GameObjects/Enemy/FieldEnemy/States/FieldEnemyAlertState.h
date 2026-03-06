#pragma once
#include "../../IEnemyState.h"

class FieldEnemy;

/// <summary>
/// 発見リアクション状態（プレイヤーを見つけた直後の「！」演出）
/// Enter → プレイヤーの方に向き直り → alertDuration 秒後 Chase へ
/// </summary>
class FieldEnemyAlertState : public IEnemyState<FieldEnemy> {
public:
	void Enter(FieldEnemy& enemy) override;
	void Update(FieldEnemy& enemy, float dt) override;
	void Exit(FieldEnemy& enemy) override;

private:
	float timer_ = 0.0f;
};