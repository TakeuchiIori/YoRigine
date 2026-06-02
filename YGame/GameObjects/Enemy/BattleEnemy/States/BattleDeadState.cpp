#include "BattleDeadState.h"
#include "Vector3.h"
#include "Object3D/Object3d.h"

void BattleDeadState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.IsInvincible() = true;
	deathTimer_ = 0.0f;
	// ディゾルブの初期化は BattleEnemy::PlayDeathEffect() 側で実施済み
}

void BattleDeadState::Update(BattleEnemy& enemy, float dt)
{
	deathTimer_ += dt;

	// 0 ~ 1 の進行度（演出時間はエディタから JsonManager 経由で調整）
	const float duration = enemy.GetDissolveDuration();
	float t = (duration > 0.0f) ? (deathTimer_ / duration) : 1.0f;
	if (t > 1.0f) t = 1.0f;

	// ディゾルブ進行（0 → 1）。ピクセル単位で discard されてエッジ発光しながら消える
	if (auto* obj = enemy.GetObject3d()) {
		obj->SetDissolveThreshold(t);
	}

	// 既存挙動：スケールも縮小（ディゾルブと併用するとシルエットも消える）
	enemy.GetWT().scale_ -= dt * 1.0f;

	// ディゾルブ完了またはスケールが消えたら非アクティブ化
	if (t >= 1.0f || enemy.GetWT().scale_.y < 0.0f) {
		enemy.SetIsAlive(false);
	}
}

void BattleDeadState::Exit([[maybe_unused]] BattleEnemy& enemy)
{
	enemy.IsInvincible() = false;

	// 次の生存ループに備えてディゾルブを無効化
	if (auto* obj = enemy.GetObject3d()) {
		obj->SetDissolveEnabled(false);
		obj->SetDissolveThreshold(0.0f);
	}
}
