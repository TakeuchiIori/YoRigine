#include "FieldEnemyChaseState.h"
#include "../FieldEnemy.h"
#include "FieldEnemySearchState.h"
#include "MathFunc.h"
#include <Debugger/Logger.h>

/// <summary>
/// 追跡状態に入ったときの初期化処理
/// </summary>
void FieldEnemyChaseState::Enter(FieldEnemy& enemy) {
	enemy.SetLogicalState(FieldEnemyState::Chase);
	chaseTimer_ = 0.0f;
	losLostTimer_ = 0.0f;
	enemy.ResetPathRefreshTimer();
	enemy.ClearNavPath();
	RequestNewPath(enemy);
	Logger("[FieldEnemyChaseState] 追跡状態に入りました。\n");
}

/// <summary>
/// 追跡状態の更新処理
/// </summary>
void FieldEnemyChaseState::Update(FieldEnemy& enemy, float dt)
{
    chaseTimer_ += dt;

    // ── 視線チェック ──────────────────────────────────────────────
	if(CanSeePlayer(enemy)) {
		losLostTimer_ = 0.0f; // 視線がある → タイマーリセット
	}
	else {
		losLostTimer_ += dt;   // 視線なし → タイマー加算
	}

    // ── 諦め判定 ──────────────────────────────────────────────────
    bool lostByLOS  = (losLostTimer_ >= kLosLostThreshold);
    bool lostByDist = (chaseTimer_ > kMinChaseTime && ShouldGiveUpChase(enemy));

    if (lostByLOS || lostByDist) {
        enemy.ClearNavPath();
        enemy.ChangeState(std::make_unique<FieldEnemySearchState>());
        return;
    }

    // 経路再計算
    enemy.UpdatePathRefreshTimer(dt);
    if (enemy.ShouldRefreshPath(enemy.GetEnemyData().pathRefreshInterval)) {
        RequestNewPath(enemy);
        enemy.ResetPathRefreshTimer();
    }

    ChasePlayer(enemy, dt);
    enemy.RotateTowardsPlayer(enemy.GetEnemyData().rotationSpeed, dt);
}
/// <summary>
/// 状態を抜ける際の処理
/// </summary>
void FieldEnemyChaseState::Exit([[maybe_unused]] FieldEnemy& enemy) {
	enemy.ClearNavPath();
}

// ── ウェイポイント追従移動 ────────────────────────────────────────────────
void FieldEnemyChaseState::ChasePlayer(FieldEnemy& enemy, float dt)
{
	if (!enemy.HasPlayer()) return;

	const auto& data = enemy.GetEnemyData();
	Vector3 enemyPos = enemy.GetPosition();

	// NavPath がある → ウェイポイント追従
	if (enemy.HasNavPath()) {
		Vector3 waypoint = enemy.GetCurrentWaypoint();
		Vector3 direction = waypoint - enemyPos;
		direction.y = 0.0f;


		float dist = Length(direction);

		char buf[256];
		sprintf_s(buf, "[Chase] wp=(%.1f,%.1f,%.1f) enemy=(%.1f,%.1f,%.1f) dist=%.2f\n",
			waypoint.x, waypoint.y, waypoint.z,
			enemyPos.x, enemyPos.y, enemyPos.z, dist);
		Logger(buf);
		// ウェイポイント到達判定（セルサイズの半分程度）
		if (dist < 0.6f) {
			enemy.AdvanceWaypoint();
			return;
		}

		direction = direction / dist;
		enemy.AddTranslate(direction * data.chaseSpeed * dt);
		return;
	}

	// NavPath なし → 直線フォールバック（壁なし環境や初回フレームなど）
	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 direction = playerPos - enemyPos;
	direction.y = 0.0f;
	float dist = Length(direction);
	if (dist > 0.5f) {
		enemy.AddTranslate((direction / dist) * data.chaseSpeed * dt);
	}
}


// ── 経路要求 ─────────────────────────────────────────────────────────────
void FieldEnemyChaseState::RequestNewPath(FieldEnemy& enemy)
{
	NavPathfinder* pf = enemy.GetNavPathfinder();
	if (!pf) return;

	// NavGrid が未初期化のときは直線追跡にフォールバックするので問題ない
	auto path = pf->FindPath(
		enemy.GetPosition(),
		enemy.GetPlayerPosition(),
		2000   // NavGridConfig.maxSearchNodes に合わせて調整
	);

	if (!path.empty()) {
		enemy.SetNavPath(path);
		Logger("[Chase] 新しい経路を設定しました。ウェイポイント数: " + std::to_string(path.size()) + "\n");
	}
	else {
		Logger("[Chase] 経路が見つかりません。NavGrid::Bake が正常か確認してください。\n");
	}
}

// FieldEnemyChaseState.cpp
bool FieldEnemyChaseState::ShouldGiveUpChase(const FieldEnemy& enemy) const
{
	if (!enemy.HasPlayer()) return true;
	const auto& data = enemy.GetEnemyData();
	float distToPlayer = Length(enemy.GetPlayerPosition() - enemy.GetPosition());
	float distToSpawn = Length(enemy.GetSpawnPosition() - enemy.GetPosition());
	if (distToPlayer > data.chaseRange * 2.0f) return true;
	if (distToSpawn > data.returnDistance)    return true;
	return false;
}

bool FieldEnemyChaseState::CanSeePlayer(const FieldEnemy& enemy) const {
	if (!enemy.HasPlayer()) return false;

	const auto& data = enemy.GetEnemyData();
	Vector3 enemyPos = enemy.GetPosition();
	Vector3 playerPos = enemy.GetPlayerPosition();
	Vector3 toPlayer = playerPos - enemyPos;

	// 簡易的なのでY軸は考慮しない（地面にいる前提）
	toPlayer.y = 0.0f;
	float distToPlayer = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;

	// ・視界距離チェック
	if(distToPlayer> data.viewDistance * data.viewDistance) {
		return false; // 視界距離外
	}

	// ・視野角チェック
	float dist = sqrtf(distToPlayer);
	if(dist < 0.001f) return false; // プレイヤーが非常に近い場合は視野角チェックをスキップ

	float rotY = enemy.GetRotationY();
	Vector3 forward = { sinf(rotY), 0.0f, cosf(rotY) };
	Vector3 dirToPlayer = toPlayer / dist;

	float dot = Dot(forward, dirToPlayer);
	float angle = acosf(std::clamp(dot, -1.0f, 1.0f)) * (180.0f / std::numbers::pi_v<float>);
	
	if (angle > data.viewAngle * 0.5f) {
		return false; // 視野角外
	}

	// ・視線チェック（NavGridのHasLineOfSightを使用）
	const NavPathfinder* pf = enemy.GetNavPathfinder();
	if (pf && pf->GetNavGrid()) {
		return pf->GetNavGrid()->HasLineOfSight(enemyPos, playerPos);
	}

	return true; // 視界内
}
