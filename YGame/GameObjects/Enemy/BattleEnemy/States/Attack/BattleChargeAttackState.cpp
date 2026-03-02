#include "BattleChargeAttackState.h"

void BattleChargeAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.8f, 0.0f, 1 }); // オレンジ色で警告

	startY_ = enemy.GetTranslate().y;

	if (enemy.GetPlayer()) {
		attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(attackDir_) > 0.01f) {
			attackDir_ = Normalize(attackDir_);
			enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
		}
	}
}

void BattleChargeAttackState::Update(BattleEnemy& enemy, float dt)
{
	const float currentTime = enemy.GetStateTimer();
	const auto& params = enemy.GetEnemyData().attackParams.charge;

	// フェーズの境界時間を計算
	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float rushEndTime = chargeEndTime + params.rushTime;
	const float totalDuration = rushEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作（地面を踏み込む） ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;

		// 沈み込む動き
		const float sinkProgress = std::sin(progress * 3.14159f); // 0→1→0の曲線
		Vector3 pos = enemy.GetTranslate();
		pos.y = startY_ - (params.stompIntensity * sinkProgress);
		enemy.SetTranslate(pos);

		// 色を激しく点滅
		const float colorPulse = 0.5f + 0.5f * std::sin(currentTime * params.anticipationColorPulseSpeed);
		enemy.SetColor({ 1, 0.5f + colorPulse * 0.3f, 0.0f, 1 });

		// プレイヤーの方を向き続ける
		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}
	}
	// === フェーズ2: チャージ（溜め） ===
	else if (currentTime < chargeEndTime) {
		// 元の高さに戻す
		Vector3 pos = enemy.GetTranslate();
		pos.y = startY_;
		enemy.SetTranslate(pos);

		// チャージ中プレイヤーを追尾
		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}

		// 色を点滅させて警告
		const float chargeTime = currentTime - anticipationEndTime;
		const float blink = std::sin(chargeTime * 10.0f) * 0.3f + 0.7f;
		enemy.SetColor({ 1, 0.5f * blink, 0.0f, 1 });
	}
	// === フェーズ3: 高速突進 ===
	else if (currentTime < rushEndTime) {
		// 高速突進して色を変更
		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
		enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * params.speedMultiplier * dt);
	}
	// === フェーズ4: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleChargeAttackState::Exit(BattleEnemy& enemy)
{
	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });

	// 高さを元に戻す
	Vector3 pos = enemy.GetTranslate();
	pos.y = startY_;
	enemy.SetTranslate(pos);
}