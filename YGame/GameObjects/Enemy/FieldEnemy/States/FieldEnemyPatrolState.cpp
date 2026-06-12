#include "FieldEnemyPatrolState.h"
#include "../FieldEnemy.h"
#include "FieldEnemyAlertState.h"
#include "MathFunc.h"
#include "Player/Player.h"
#include <Debugger/Logger.h>
#include <Object3D/ObjectManager.h>
#include <Collision/AABB/AABBCollider.h>
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Systems/Navigation/VisionSystem.h>


#include <random>
#include <numbers>

/// <summary>
/// 巡回状態に入った際の初期化処理
/// </summary>
void FieldEnemyPatrolState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Patrol);
	GenerateNewPatrolTarget(enemy);
}

/// <summary>
/// 巡回状態の更新処理
/// </summary>
void FieldEnemyPatrolState::Update(FieldEnemy& enemy, float dt) {
	// ターゲット到達チェック
	if (HasReachedTarget(enemy)) {
		GenerateNewPatrolTarget(enemy);
	}

	// ターゲットに向かって移動
	MoveTowardsTarget(enemy, dt);

	// プレイヤー検知
	CheckForPlayer(enemy);
}

/// <summary>
/// 状態を抜ける際の処理
/// </summary>
void FieldEnemyPatrolState::Exit([[maybe_unused]] FieldEnemy& enemy) {}

void FieldEnemyPatrolState::GenerateNewPatrolTarget(FieldEnemy& enemy) {
	std::random_device rd;
	std::mt19937 gen(rd());
	const auto& data = enemy.GetEnemyData();
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> radiusDist(2.0f, data.patrolRadius);

	Vector3 spawnPos = enemy.GetSpawnPosition();
	NavPathfinder* pf = enemy.GetNavPathfinder();

	// 壁の中など unreachable な target を引いたら直線フォールバックで壁に突っ込んでしまう。
	// (= 「敵が他のシーンエディタで配置したオブジェクトにめり込む」現象の主犯)
	// reachable な target が見つかるまで何度かリトライする。
	constexpr int kMaxRetry = 6;
	for (int i = 0; i < kMaxRetry; ++i) {
		float angle  = angleDist(gen);
		float radius = radiusDist(gen);
		Vector3 candidate = spawnPos + Vector3(
			radius * std::cosf(angle), 0.0f, radius * std::sinf(angle));

		if (pf) {
			auto path = pf->FindPath(enemy.GetPosition(), candidate);
			if (!path.empty()) {
				enemy.SetPatrolTarget(candidate);
				enemy.SetNavPath(path);
				return;
			}
		} else {
			// NavPathfinder が無い構成では retry しても変わらないので 1 回で抜ける
			enemy.SetPatrolTarget(candidate);
			enemy.ClearNavPath();
			return;
		}
	}

	// 全 retry が unreachable: 壁めり込みを避けるためスポーン位置に留まる
	enemy.SetPatrolTarget(spawnPos);
	enemy.ClearNavPath();
}

/// <summary>
/// 現在の目標地点に到達したかを判定する
/// </summary>
bool FieldEnemyPatrolState::HasReachedTarget(const FieldEnemy& enemy) const {
	// NavPath が残っている間はウェイポイント側で管理するので false
	if (enemy.HasNavPath()) return false;

	Vector3 dir = enemy.GetPatrolTarget() - enemy.GetPosition();
	dir.y = 0.0f;
	return Length(dir) < 0.5f;
}

void FieldEnemyPatrolState::MoveTowardsTarget(FieldEnemy& enemy, float dt) {
	const auto& data = enemy.GetEnemyData();

	// NavPath がある → ウェイポイント追従
	if (enemy.HasNavPath()) {
		Vector3 waypoint = enemy.GetCurrentWaypoint();
		Vector3 direction = waypoint - enemy.GetPosition();
		direction.y = 0.0f;
		float dist = Length(direction);

		if (dist < 0.6f) {
			enemy.AdvanceWaypoint();
			return;
		}
		direction = direction / dist;
		enemy.AddTranslate(direction * data.patrolSpeed * dt);
		enemy.RotateTowardsDirection(direction, data.rotationSpeed, dt);
		return;
	}

	// フォールバック：既存の直線移動
	Vector3 direction = enemy.GetPatrolTarget() - enemy.GetPosition();
	direction.y = 0.0f;
	float dist = Length(direction);
	if (dist > 0.1f) {
		direction = direction / dist;
		enemy.AddTranslate(direction * data.patrolSpeed * dt);
		enemy.RotateTowardsDirection(direction, data.rotationSpeed, dt);
	}
}

/// <summary>
/// プレイヤー検知 → Alert 状態へ遷移
/// （直接 Chase に飛ばさず、発見リアクションを挟む）
/// </summary>
void FieldEnemyPatrolState::CheckForPlayer(FieldEnemy& enemy) {
	Player* player = enemy.GetPlayer();
	if (!player) return;

	Vector3 enemyPos = enemy.GetPosition();
	Vector3 playerPos = player->GetWorldPosition();
	Vector3 toPlayer = playerPos - enemyPos;
	toPlayer.y = 0.0f;

	float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
	bool isDetected = false;

	// A. 視界判定：DrawViewCone と同じ計算
	float viewDist = enemy.GetEnemyData().viewDistance;
	if (distSq <= viewDist * viewDist) {
		float rotY = enemy.GetRotationY();
		Vector3 forward = { sinf(rotY), 0.0f, cosf(rotY) };

		float dist = sqrtf(distSq);
		if (dist > 0.001f) {
			Vector3 dirToPlayer = toPlayer / dist;
			float dot = Dot(forward, dirToPlayer);
			float angle = acosf(std::clamp(dot, -1.0f, 1.0f))
				* (180.0f / std::numbers::pi_v<float>);

			if (angle <= enemy.GetEnemyData().viewAngle * 0.5f) {
				isDetected = VisionSystem::HasLineOfSight(enemyPos, playerPos);
			}
		}
	}

	// B. 足音判定（走っている時のみ）
	if (!isDetected && player->GetMovement()->IsRunning()) {
		float noiseDist = enemy.GetEnemyData().noiseDetectionRange;
		if (distSq <= noiseDist * noiseDist) {
			isDetected = true;
		}
	}

	if (isDetected) {
		// Chase に直接行かず、Alert（発見リアクション）を挟む
		enemy.ChangeState(std::make_unique<FieldEnemyAlertState>());
	}
}