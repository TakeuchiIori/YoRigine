#include "BattleIdleState.h"
#include "BattleApproachState.h"

/// <summary>
/// ランダムなオフセット位置を生成
/// </summary>
Vector3 BattleIdleState::GetRandomOffset(float radius)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
	std::uniform_real_distribution<float> dist(0.5f, radius);

	float angle = angleDist(gen);
	float len = dist(gen);
	return Vector3(std::cos(angle) * len, 0.0f, std::sin(angle) * len);
}

/// <summary>
/// アイドル状態開始処理
/// </summary>
void BattleIdleState::Enter(BattleEnemy& enemy)
{
	enemy.ResetStateTimer();
	Vector3 offset = GetRandomOffset();
	enemy.SetTargetPosition(enemy.GetTranslate() + offset);
}

/// <summary>
/// アイドル状態更新処理
/// </summary>
void BattleIdleState::Update(BattleEnemy& enemy, float dt)
{
	Vector3 pos = enemy.GetTranslate();
	Vector3 target = enemy.GetTargetPosition();
	Vector3 dir = target - pos;
	float dist = Length(dir);

	// 目標地点に到達したら新しい目的地を設定
	if (dist < enemy.GetArrivalThreshold()) {
		enemy.SetTargetPosition(pos + GetRandomOffset());
	} else {
		// ゆっくりランダム徘徊
		dir = Normalize(dir);
		enemy.AddTranslate(dir * enemy.GetEnemyData().moveSpeed * 0.5f * dt);
		enemy.SetRotationY(std::atan2(dir.x, dir.z));
	}

	// プレイヤーが近づいたら接近状態へ
	if (enemy.GetPlayer()) {
		float dToPlayer = Length(enemy.GetPlayerPosition() - pos);
		if (dToPlayer < enemy.GetEnemyData().approachStateRange) {
			enemy.ChangeState(std::make_unique<BattleApproachState>());
		}
	}
}

/// <summary>
/// アイドル状態終了処理
/// </summary>
void BattleIdleState::Exit([[maybe_unused]] BattleEnemy& enemy)
{
}
