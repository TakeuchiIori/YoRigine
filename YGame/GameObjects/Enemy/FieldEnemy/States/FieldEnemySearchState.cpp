#include "FieldEnemySearchState.h"
#include "../FieldEnemy.h"
#include "FieldEnemyPatrolState.h"
#include "FieldEnemyChaseState.h"
#include "MathFunc.h"
#include <numbers>
#include <cmath>
#include <Debugger/Logger.h>
#include <Object3D/ObjectManager.h>
#include <Collision/AABB/AABBCollider.h>
#include <Collision/Core/CollisionTypeIdDef.h>

namespace {

	bool RayIntersectsAABB2D(const Vector3& from, const Vector3& to, const AABB& aabb) {
		float dx = to.x - from.x, dz = to.z - from.z;
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
/// 索敵開始：現在の向きを記録してスウィープ周期を計算する
/// </summary>
void FieldEnemySearchState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Search);
	timer_ = 0.0f;

	// 見失った瞬間の向きを基準角とする
	entryAngle_ = enemy.GetRotationY();

	// スウィープ 1 往復（左→右）を searchDuration の半分で行う
	const auto& data = enemy.GetEnemyData();
	sweepPeriod_ = data.searchDuration * 0.5f;

	// ここに「？」エフェクト・SE 再生などを追加できます
	Logger("[FieldEnemy] ？ プレイヤーを見失った…索敵中\n");
}

/// <summary>
/// 索敵更新
/// 左右を見渡す sin 波スウィープを行い、視界内でプレイヤーを発見したら Chase に再遷移
/// searchDuration 経過後 → Patrol へ
/// </summary>
void FieldEnemySearchState::Update(FieldEnemy& enemy, float dt) {
	timer_ += dt;

	const auto& data = enemy.GetEnemyData();

	// --- 左右スウィープ（sin 波で自然な往復） ---
	// searchSweepAngle をラジアンに変換して左右に振る
	float sweepRad = data.searchSweepAngle * (std::numbers::pi_v<float> / 180.0f);
	float phase = (timer_ / sweepPeriod_) * std::numbers::pi_v<float>; // 周期ごとに π 進む
	float targetAngle = entryAngle_ + sweepRad * std::sin(phase);

	// 補間回転でなめらかに首を振る
	enemy.RotateTowards(targetAngle, data.rotationSpeed, dt);

	// --- プレイヤー再発見チェック（視界判定） ---
	Player* player = enemy.GetPlayer();
	if (player) {
		Vector3 enemyPos = enemy.GetPosition();
		Vector3 playerPos = enemy.GetPlayerPosition();
		Vector3 toPlayer = playerPos - enemyPos;
		toPlayer.y = 0.0f;

		float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
		float viewDist = data.viewDistance;

		if (distSq <= viewDist * viewDist) {
			float rotY = enemy.GetRotationY();
			Vector3 forward = { sinf(rotY), 0.0f, cosf(rotY) };

			float dist = sqrtf(distSq);
			if (dist > 0.001f) {
				Vector3 dirToPlayer = toPlayer / dist;
				float dot = Dot(forward, dirToPlayer);
				float angle = acosf(std::clamp(dot, -1.0f, 1.0f))
					* (180.0f / std::numbers::pi_v<float>);

				if (angle <= data.viewAngle * 0.5f) {
					if (!IsBlockedByObstacle(enemyPos, playerPos)) {
						Logger("[FieldEnemy] 索敵中に再発見！追跡再開\n");
						enemy.ChangeState(std::make_unique<FieldEnemyChaseState>());
						return;
					}
				}
			}
		}
	}

	// --- searchDuration 経過 → 巡回へ ---
	if (timer_ >= data.searchDuration) {
		Logger("[FieldEnemy] 見つからなかった…巡回に戻ります\n");
		enemy.ChangeState(std::make_unique<FieldEnemyPatrolState>());
	}
}

/// <summary>
/// 索敵終了
/// </summary>
void FieldEnemySearchState::Exit([[maybe_unused]] FieldEnemy& enemy) {}