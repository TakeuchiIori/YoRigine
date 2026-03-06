#include "FieldEnemyChaseState.h"
#include "../FieldEnemy.h"
#include "FieldEnemySearchState.h"
#include "MathFunc.h"

/// <summary>
/// 追跡状態に入ったときの初期化処理
/// </summary>
void FieldEnemyChaseState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Chase);
}

/// <summary>
/// 追跡状態の更新処理
/// </summary>
void FieldEnemyChaseState::Update(FieldEnemy& enemy, float dt) {
	// プレイヤーがいない、または範囲外 → 索敵状態へ（Patrol に直接戻さない）
	if (ShouldGiveUpChase(enemy)) {
		enemy.ChangeState(std::make_unique<FieldEnemySearchState>());
		return;
	}

	ChasePlayer(enemy, dt);

	// 補間回転でプレイヤーを追う
	const auto& data = enemy.GetEnemyData();
	enemy.RotateTowardsPlayer(data.rotationSpeed, dt);
}

/// <summary>
/// 状態を抜ける際の処理
/// </summary>
void FieldEnemyChaseState::Exit([[maybe_unused]] FieldEnemy& enemy) {}

/// <summary>
/// プレイヤーを追跡する処理（移動のみ、回転は Update 側で行う）
/// </summary>
void FieldEnemyChaseState::ChasePlayer(FieldEnemy& enemy, float dt) {
	if (!enemy.HasPlayer()) return;

	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 enemyPos = enemy.GetPosition();
	Vector3 direction = playerPos - enemyPos;
	direction.y = 0.0f;

	float distance = Length(direction);
	if (distance > 0.5f) {
		direction = direction / distance;
		const auto& data = enemy.GetEnemyData();
		enemy.AddTranslate(direction * data.chaseSpeed * dt);
	}
}

/// <summary>
/// 追跡を諦めて索敵へ移行すべきかを判定する
/// </summary>
bool FieldEnemyChaseState::ShouldGiveUpChase(const FieldEnemy& enemy) const {
	if (!enemy.HasPlayer()) return true;

	const auto& data = enemy.GetEnemyData();
	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 enemyPos = enemy.GetPosition();
	Vector3 spawnPos = enemy.GetSpawnPosition();

	float distToPlayer = Length(playerPos - enemyPos);
	float distToSpawn = Length(spawnPos - enemyPos);

	if (distToPlayer > data.chaseRange * 1.5f) return true;
	if (distToSpawn > data.returnDistance)     return true;

	return false;
}