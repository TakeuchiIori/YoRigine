#include "FieldEnemyAlertState.h"
#include "../FieldEnemy.h"
#include "FieldEnemyChaseState.h"
#include <Debugger/Logger.h>

/// <summary>
/// 発見リアクション開始
/// </summary>
void FieldEnemyAlertState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Alert);
	timer_ = 0.0f;

	// ここに「！」エフェクト・SE 再生などを追加できます
	Logger("[FieldEnemy] ！ プレイヤーを発見！\n");
}

/// <summary>
/// 発見リアクション更新
/// プレイヤーの方へ素早く向き直り、一定時間後 Chase へ遷移する
/// </summary>
void FieldEnemyAlertState::Update(FieldEnemy& enemy, float dt) {
	timer_ += dt;

	// 発見した相手に向かって素早く向き直る（通常の2倍速）
	const auto& data = enemy.GetEnemyData();
	enemy.RotateTowardsPlayer(data.rotationSpeed * 2.0f, dt);

	// alertDuration 経過後 → Chase へ
	if (timer_ >= data.alertDuration) {
		enemy.ChangeState(std::make_unique<FieldEnemyChaseState>());
	}
}

/// <summary>
/// 発見リアクション終了
/// </summary>
void FieldEnemyAlertState::Exit([[maybe_unused]] FieldEnemy& enemy) {}