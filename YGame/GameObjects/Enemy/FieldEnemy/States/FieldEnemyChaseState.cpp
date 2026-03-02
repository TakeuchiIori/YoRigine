#include "FieldEnemyChaseState.h"
#include "../FieldEnemy.h"
#include "FieldEnemyPatrolState.h"
#include "MathFunc.h"

/// <summary>
/// 追跡状態に入ったときの初期化処理
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
void FieldEnemyChaseState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Chase);
}

/// <summary>
/// 追跡状態の更新処理
/// </summary>
/// <param name="enemy">更新対象のフィールド敵</param>
/// <param name="dt">経過時間（デルタタイム）</param>
void FieldEnemyChaseState::Update(FieldEnemy& enemy, float dt) {
	// プレイヤーがいない、または遠すぎる場合は巡回に戻る
	if (ShouldReturnToPatrol(enemy)) {
		enemy.ChangeState(std::make_unique<FieldEnemyPatrolState>());
		return;
	}

	// プレイヤーを追跡
	ChasePlayer(enemy, dt);

	// プレイヤーの方を向く
	FacePlayer(enemy);
}

/// <summary>
/// 状態を抜ける際の処理
/// </summary>
/// <param name="enemy">対象のフィールド敵（未使用）</param>
void FieldEnemyChaseState::Exit([[maybe_unused]] FieldEnemy& enemy) {
	// 必要に応じてクリーンアップ
}

/// <summary>
/// プレイヤーを追跡する処理
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
/// <param name="dt">経過時間（デルタタイム）</param>
void FieldEnemyChaseState::ChasePlayer(FieldEnemy& enemy, float dt) {
	if (!enemy.HasPlayer()) {
		return;
	}

	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 enemyPos = enemy.GetPosition();

	Vector3 direction = playerPos - enemyPos;
	direction.y = 0.0f;

	float distance = Length(direction);

	if (distance > 0.5f) {
		// 正規化して移動
		direction = direction / distance;

		const auto& data = enemy.GetEnemyData();
		Vector3 movement = direction * data.chaseSpeed * dt;
		enemy.AddTranslate(movement);
	}
}

/// <summary>
/// 追跡を中止し、巡回に戻るべきかを判定する
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
/// <returns>true: 巡回に戻るべき / false: 継続追跡</returns>
bool FieldEnemyChaseState::ShouldReturnToPatrol(const FieldEnemy& enemy) const {
	if (!enemy.HasPlayer()) {
		return true;
	}

	const auto& data = enemy.GetEnemyData();
	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 enemyPos = enemy.GetPosition();
	Vector3 spawnPos = enemy.GetSpawnPosition();

	// プレイヤーとの距離
	float distanceToPlayer = Length(playerPos - enemyPos);

	// スポーン地点からの距離
	float distanceToSpawn = Length(spawnPos - enemyPos);

	// プレイヤーが追跡範囲外、またはスポーン地点から遠すぎる場合
	if (distanceToPlayer > data.chaseRange * 1.5f) {
		return true;
	}

	if (distanceToSpawn > data.returnDistance) {
		return true;
	}

	return false;
}

/// <summary>
/// 敵をプレイヤーの方向に向ける処理
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
void FieldEnemyChaseState::FacePlayer(FieldEnemy& enemy) {
	if (!enemy.HasPlayer()) {
		return;
	}

	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 enemyPos = enemy.GetPosition();

	Vector3 direction = playerPos - enemyPos;
	direction.y = 0.0f;

	float distance = Length(direction);

	if (distance > 0.1f) {
		direction = direction / distance;
		float angle = std::atan2(direction.x, direction.z);
		enemy.SetRotationY(angle);
	}
}
