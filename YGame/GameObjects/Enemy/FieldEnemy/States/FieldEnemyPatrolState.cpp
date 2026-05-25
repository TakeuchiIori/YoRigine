#include "FieldEnemyPatrolState.h"
#include "../FieldEnemy.h"
#include "FieldEnemyAlertState.h"
#include "MathFunc.h"
#include <random>
#include <numbers>
#include "Player/Player.h"
#include <Debugger/Logger.h>
#include <Object3D/ObjectManager.h>
#include <Collision/AABB/AABBCollider.h>
#include <Collision/Core/CollisionTypeIdDef.h>

namespace {

	bool RayIntersectsAABB2D(const Vector3& from, const Vector3& to, const AABB& aabb) {
		float dx = to.x - from.x;
		float dz = to.z - from.z;
		float maxDist = sqrtf(dx * dx + dz * dz);
		if (maxDist < 1e-6f) return false;
		dx /= maxDist; dz /= maxDist;
		float tmin = 0.0f, tmax = maxDist;
		if (fabsf(dx) < 1e-6f) { if (from.x < aabb.min.x || from.x > aabb.max.x) return false; } else { float t1 = (aabb.min.x - from.x) / dx, t2 = (aabb.max.x - from.x) / dx; if (t1 > t2)std::swap(t1, t2); tmin = std::max(tmin, t1); tmax = std::min(tmax, t2); if (tmin > tmax)return false; }
		if (fabsf(dz) < 1e-6f) { if (from.z < aabb.min.z || from.z > aabb.max.z) return false; } else { float t1 = (aabb.min.z - from.z) / dz, t2 = (aabb.max.z - from.z) / dz; if (t1 > t2)std::swap(t1, t2); tmin = std::max(tmin, t1); tmax = std::min(tmax, t2); if (tmin > tmax)return false; }
		return tmax >= 0.0f;
	}

	bool IsBlockedByObstacle(const Vector3& from, const Vector3& to) {
		ObjectManager* om = ObjectManager::GetInstance();
		if (!om) return false;
		for (const auto* obj : om->GetAllActiveObjects()) {
			if (!obj || !obj->collider || !obj->colliderEnabled) continue;
			const auto* tmpl = om->FindTemplate(obj->modelName);
			if (!tmpl) continue;
			if (tmpl->typeId != CollisionTypeIdDef::kNavObstacle &&
				tmpl->typeId != CollisionTypeIdDef::kStaticWall) continue;
			const auto* aabb = dynamic_cast<const AABBCollider*>(obj->collider.get());
			if (aabb && RayIntersectsAABB2D(from, to, aabb->GetAABB())) return true;
		}
		return false;
	}

} // anonymous namespace

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
	// 既存のランダム生成ロジックで目標座標を決める
	std::random_device rd;
	std::mt19937 gen(rd());
	const auto& data = enemy.GetEnemyData();
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * std::numbers::pi_v<float>);
	std::uniform_real_distribution<float> radiusDist(2.0f, data.patrolRadius);

	float angle = angleDist(gen);
	float radius = radiusDist(gen);
	Vector3 spawnPos = enemy.GetSpawnPosition();
	Vector3 newTarget = spawnPos + Vector3(
		radius * std::cosf(angle), 0.0f, radius * std::sinf(angle));

	enemy.SetPatrolTarget(newTarget);

	// ── ここで NavPath を計算 ──────────────────────────────────────────
	NavPathfinder* pf = enemy.GetNavPathfinder();
	if (pf) {
		auto path = pf->FindPath(enemy.GetPosition(), newTarget);
		if (!path.empty()) {
			enemy.SetNavPath(path);
			return;
		}
	}
	// NavPath 取れなかった場合は直線移動にフォールバック
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
				isDetected = !IsBlockedByObstacle(enemyPos, playerPos);
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