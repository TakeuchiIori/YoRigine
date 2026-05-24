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
	enemy.ResetPathRefreshTimer();
	enemy.ClearNavPath();
	RequestNewPath(enemy);
}

/// <summary>
/// 追跡状態の更新処理
/// </summary>
void FieldEnemyChaseState::Update(FieldEnemy& enemy, float dt)
{
    chaseTimer_ += dt;

    // ── 視線チェック ──────────────────────────────────────────────
    const NavPathfinder* pf = enemy.GetNavPathfinder();
    if (pf && pf->GetNavGrid()) {
        bool hasLOS = pf->GetNavGrid()->HasLineOfSight(
            enemy.GetPosition(), enemy.GetPlayerPosition());

        if (!hasLOS) {
            losLostTimer_ += dt;
        } else {
            losLostTimer_ = 0.0f;
        }
    }

    // ── 諦め判定 ──────────────────────────────────────────────────
    bool lostByLOS  = (losLostTimer_ >= kLosLostThreshold);
    bool lostByDist = (chaseTimer_ > kMinChaseTime && ShouldGiveUpChase(enemy));

    if (lostByLOS || lostByDist) {
        //ogger("[Chase] 追跡終了（%s）\n", lostByLOS ? "視線断絶" : "距離超過");
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