#include "BattleRushAttackState.h"
#include "../BattleIdleState.h"
#include <cmath>

/// <summary>
/// 攻撃状態開始処理
/// </summary>
void BattleRushAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 1, 0.0f, 1 }); // 黄色で予備動作を示す
	dirLocked_ = false;

	// 予備動作の開始位置を記録
	anticipationStartPos_ = enemy.GetTranslate();

	// 攻撃方向を決定
	if (enemy.GetPlayer()) {
		attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(attackDir_) > 0.01f) {
			attackDir_ = Normalize(attackDir_);
			enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
		}
	}
}

/// <summary>
/// 攻撃状態更新処理
/// </summary>
void BattleRushAttackState::Update(BattleEnemy& enemy, float dt) {
	const float currentTime = enemy.GetStateTimer();
	const auto& params = enemy.GetEnemyData().attackParams.rush;

	// フェーズの境界時間を計算
	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float rushEndTime = chargeEndTime + params.rushTime;
	const float totalDuration = rushEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作（後ろに引く） ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;

		// 後ろに引く動き（イージングを使用してスムーズに）
		const float easeProgress = 1.0f - std::pow(1.0f - progress, 3.0f); // ease-out cubic
		const Vector3 backwardOffset = -attackDir_ * params.anticipationDistance * easeProgress;
		enemy.SetTranslate(anticipationStartPos_ + backwardOffset);

		// 色を点滅させて警告
		const float colorPulse = 0.5f + 0.5f * std::sin(currentTime * 12.0f);
		enemy.SetColor({ 1, colorPulse, 0.0f, 1 });
	}
	// === フェーズ2: チャージ（溜め） ===
	else if (currentTime < chargeEndTime) {
		// チャージ中もプレイヤーを追尾
		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}

		// 色を赤く変化
		const float chargeProgress = (currentTime - anticipationEndTime) / params.chargeTime;
		const float redIntensity = 0.5f + 0.5f * chargeProgress;
		enemy.SetColor({ 1, redIntensity * 0.3f, 0.0f, 1 });
	}
	// === フェーズ3: 突進 ===
	else if (currentTime < rushEndTime) {
		enemy.SetColor({ 1, 0.0f, 0.0f, 1 }); // 真っ赤
		enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * params.speedMultiplier * dt);
	}
	// === フェーズ4: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

/// <summary>
/// 攻撃状態終了処理
/// </summary>
void BattleRushAttackState::Exit(BattleEnemy& enemy) {
	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}