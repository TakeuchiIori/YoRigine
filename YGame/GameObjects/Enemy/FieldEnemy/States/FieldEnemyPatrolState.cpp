#include "FieldEnemyPatrolState.h"
#include "../FieldEnemy.h"
#include "FieldEnemyChaseState.h"
#include "MathFunc.h"
#include <random>
#include <numbers>
#include "Player/Player.h"
#include <Debugger/Logger.h>

/// <summary>
/// 巡回状態に入った際の初期化処理
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
void FieldEnemyPatrolState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Patrol);
	GenerateNewPatrolTarget(enemy);
}

/// <summary>
/// 巡回状態の更新処理
/// </summary>
/// <param name="enemy">更新対象のフィールド敵</param>
/// <param name="dt">経過時間（デルタタイム）</param>
void FieldEnemyPatrolState::Update(FieldEnemy& enemy, float dt) {

	//// 1. プレイヤーのポインタを取得
	//Player* player = enemy.GetPlayer();
	//if (!player) return;

	//// --- プレイヤー検知ロジック ---

	//Vector3 enemyPos = enemy.GetPosition();
	//Vector3 playerPos = player->GetWorldPosition();
	//Vector3 toPlayer = playerPos - enemyPos;
	//float distSq = Length(toPlayer);

	//bool isDetected = false;

	//// A. 視界判定（視野角内か？）
	//if (distSq <= enemy.GetEnemyData().viewDistance * enemy.GetEnemyData().viewDistance) {
	//	// 敵の前方ベクトル（Z軸）を取得
	//	const Matrix4x4& mat = enemy.GetWT().matWorld_;
	//	Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
	//	forward = Normalize(forward);

	//	Vector3 dirToPlayer = Normalize(toPlayer);
	//	float dot = Dot(forward, dirToPlayer);

	//	// 内積から角度(ラジアン)を求め、度数に変換
	//	float angle = acosf(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / 3.141592f);

	//	if (angle <= enemy.GetEnemyData().viewAngle * 0.5f) {
	//		isDetected = true; // 視界内で見つかった
	//	}
	//}

	//// B. 足音判定（視野外でも走っていればバレる）
	//if (!isDetected && player->GetMovement()->IsRunning()) {
	//	if (distSq <= enemy.GetEnemyData().noiseDetectionRange * enemy.GetEnemyData().noiseDetectionRange) {
	//		isDetected = true; // 視野外だが走る音でバレた
	//	}
	//}

	//// --- 状態遷移 ---
	//if (isDetected) {
	//	Logger("[FieldEnemy] 見つけた！追跡状態へ移行します。\n");
	//	enemy.ChangeState(std::make_unique<FieldEnemyChaseState>()); // ChaseStateへ切り替え
	//	return;
	//}

	// 新しい巡回ターゲットが必要かチェック
	if (HasReachedTarget(enemy)) {
		GenerateNewPatrolTarget(enemy);
	}

	// ターゲットに向かって移動
	MoveTowardsTarget(enemy, dt);

	// プレイヤーが範囲内にいるかチェック
	CheckForPlayer(enemy);
}

/// <summary>
/// 状態を抜ける際の処理
/// </summary>
/// <param name="enemy">対象のフィールド敵（未使用）</param>
void FieldEnemyPatrolState::Exit([[maybe_unused]] FieldEnemy& enemy) {
	// 必要に応じてクリーンアップ
}

/// <summary>
/// 新しい巡回目標地点を生成する
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
void FieldEnemyPatrolState::GenerateNewPatrolTarget(FieldEnemy& enemy) {
	std::random_device rd;
	std::mt19937 gen(rd());

	const auto& data = enemy.GetEnemyData();
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> radiusDist(2.0f, data.patrolRadius);

	float angle = angleDist(gen);
	float radius = radiusDist(gen);

	Vector3 spawnPos = enemy.GetSpawnPosition();
	Vector3 newTarget = spawnPos + Vector3(
		radius * std::cos(angle),
		0.0f,
		radius * std::sin(angle)
	);

	enemy.SetPatrolTarget(newTarget);
}

/// <summary>
/// 現在の目標地点に到達したかを判定する
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
/// <returns>true: 到達済み / false: 未到達</returns>
bool FieldEnemyPatrolState::HasReachedTarget(const FieldEnemy& enemy) const {
	Vector3 currentPos = enemy.GetPosition();
	Vector3 target = enemy.GetPatrolTarget();

	Vector3 direction = target - currentPos;
	direction.y = 0.0f;

	float distance = Length(direction);
	return distance < 0.5f;
}

/// <summary>
/// 目標地点に向かって移動する処理
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
/// <param name="dt">経過時間（デルタタイム）</param>
void FieldEnemyPatrolState::MoveTowardsTarget(FieldEnemy& enemy, float dt) {
	Vector3 currentPos = enemy.GetPosition();
	Vector3 target = enemy.GetPatrolTarget();

	Vector3 direction = target - currentPos;
	direction.y = 0.0f;

	float distance = Length(direction);

	if (distance > 0.1f) {
		// 正規化して移動
		direction = direction / distance;

		const auto& data = enemy.GetEnemyData();
		Vector3 movement = direction * data.patrolSpeed * dt;
		enemy.AddTranslate(movement);

		// 移動方向を向く
		float angle = std::atan2(direction.x, direction.z);
		enemy.SetRotationY(angle);
	}
}

/// <summary>
/// プレイヤーを検知し、追跡状態に遷移するかを判定
/// </summary>
/// <param name="enemy">対象のフィールド敵</param>
//void FieldEnemyPatrolState::CheckForPlayer(FieldEnemy& enemy) {
//	if (!enemy.HasPlayer()) {
//		return;
//	}
//
//	Vector3 playerPos = enemy.GetPlayerPosition();
//	Vector3 enemyPos = enemy.GetPosition();
//
//	float distanceToPlayer = Length(playerPos - enemyPos);
//
//	const auto& data = enemy.GetEnemyData();
//	if (distanceToPlayer < data.chaseRange) {
//		// Chase状態に遷移
//		enemy.ChangeState(std::make_unique<FieldEnemyChaseState>());
//	}
//}
void FieldEnemyPatrolState::CheckForPlayer(FieldEnemy& enemy) {
	Player* player = enemy.GetPlayer();
	if (!player) return;

	Vector3 enemyPos = enemy.GetPosition();
	Vector3 playerPos = player->GetWorldPosition();
	Vector3 toPlayer = playerPos - enemyPos;
	// 距離の平方は LengthSq があればそれを使うのが高速です
	float distSq = (toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);

	bool isDetected = false;

	// A. 視界判定
	float viewDist = enemy.GetEnemyData().viewDistance;
	if (distSq <= viewDist * viewDist) {
		// 前方ベクトル算出
		const Matrix4x4& mat = enemy.GetWT().matWorld_;
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
		forward = Normalize(forward);

		Vector3 dirToPlayer = Normalize(toPlayer);
		float dot = Dot(forward, dirToPlayer);

		// クランプして誤差回避しつつ角度算出
		float angle = acosf(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / 3.141592f);

		if (angle <= enemy.GetEnemyData().viewAngle * 0.5f) {
			isDetected = true;
		}
	}

	// B. 足音判定（走っている時のみ）
	// ※GetMovement()の実装に合わせて調整してください
	if (!isDetected && player->GetMovement()->IsRunning()) {
		float noiseDist = enemy.GetEnemyData().noiseDetectionRange;
		if (distSq <= noiseDist * noiseDist) {
			isDetected = true;
		}
	}

	// --- 状態遷移 ---
	if (isDetected) {
		Logger("[FieldEnemy] プレイヤーを検知！追跡を開始します。\n");
		// ここで状態を変えるので、これ以降の Patrol 処理は不要になる
		enemy.ChangeState(std::make_unique<FieldEnemyChaseState>());
	}
}