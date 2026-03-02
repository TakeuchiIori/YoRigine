#include "BattleComboAttackState.h"
#include "Player/Player.h"

void BattleComboAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.0f, 1, 1 }); // マゼンタ色で予備動作を示す
	comboCount_ = 0;
	hasPerformedAnticipation_ = false;
	anticipationStartPos_ = enemy.GetTranslate();
}

void BattleComboAttackState::Update(BattleEnemy& enemy, float dt) {
	const auto& params = enemy.GetEnemyData().attackParams.combo;
	const float currentTime = enemy.GetStateTimer();
	const int maxHits = 3;

	// 予備動作の時間
	const float anticipationEndTime = params.anticipationTime;
	const float totalComboTime = anticipationEndTime + (params.phaseDuration * maxHits);
	const float totalDuration = totalComboTime + params.cooldownTime;

	// === フェーズ1: 予備動作（素早く後退） ===
	if (currentTime < anticipationEndTime && !hasPerformedAnticipation_) {
		const float progress = currentTime / params.anticipationTime;

		// プレイヤーとは逆方向に後退
		Player* player = enemy.GetPlayer();
		if (player) {
			Vector3 toPlayer = player->GetTranslate() - anticipationStartPos_;
			if (Length(toPlayer) > 0.01f) {
				Vector3 backwardDir = -Normalize(toPlayer);

				// イージングを使って後退（ease-out）
				const float easeProgress = 1.0f - std::pow(1.0f - progress, 2.0f);
				const Vector3 backstepOffset = backwardDir * params.anticipationBackstepDistance * easeProgress;
				enemy.SetTranslate(anticipationStartPos_ + backstepOffset);

				// プレイヤーの方を向き続ける
				enemy.SetRotationY(std::atan2(toPlayer.x, toPlayer.z));
			}
		}

		// 色を変化
		const float colorIntensity = params.anticipationColorIntensity + (1.0f - params.anticipationColorIntensity) * progress;
		enemy.SetColor({ 1, 0.0f, colorIntensity, 1 });

		// 予備動作完了フラグ
		if (progress >= 0.99f) {
			hasPerformedAnticipation_ = true;
		}
	}
	// === フェーズ2: コンボ攻撃 ===
	else if (currentTime >= anticipationEndTime && currentTime < totalComboTime) {
		const float comboTime = currentTime - anticipationEndTime;
		const int currentHit = static_cast<int>(comboTime / params.phaseDuration);
		const float timeInPhase = std::fmod(comboTime, params.phaseDuration);
		comboCount_ = currentHit;

		// サブフェーズ判定
		const bool isSubCharging = (timeInPhase < params.subChargeTime);
		const bool isSubRushing = (!isSubCharging && timeInPhase < params.subChargeTime + params.subRushTime);

		if (isSubCharging) {
			//--- プレイヤーの方を向く ---//
			UpdateOrientation(enemy);

			// チャージ中の色変化
			const float chargeProgress = timeInPhase / params.subChargeTime;
			enemy.SetColor({ 1, 0.0f, 0.5f + 0.5f * chargeProgress, 1 });
		}
		else if (isSubRushing) {
			//--- 突進する ---//
			const float currentMultiplier = params.rushSpeedMultiplier + (comboCount_ * 1.0f);
			ExecuteRush(enemy, currentMultiplier, dt);

			// 突進中は赤色
			enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
		}
	}
	// === フェーズ3: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleComboAttackState::Exit(BattleEnemy& enemy)
{
	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}

// --- 補助関数の定義 ---
void BattleComboAttackState::UpdateOrientation(BattleEnemy& enemy) {
	Player* player = enemy.GetPlayer();
	if (!player) return;

	Vector3 toPlayer = player->GetTranslate() - enemy.GetTranslate();
	if (Length(toPlayer) > 0.01f) {
		Vector3 dir = Normalize(toPlayer);
		enemy.SetRotationY(std::atan2(dir.x, dir.z));
	}
}

void BattleComboAttackState::ExecuteRush(BattleEnemy& enemy, float speedMultiplier, float dt) {
	Player* player = enemy.GetPlayer();
	if (!player) return;

	// ターゲットへの方向を計算
	Vector3 toPlayer = player->GetTranslate() - enemy.GetTranslate();
	if (Length(toPlayer) > 0.01f) {
		Vector3 dir = Normalize(toPlayer);

		// 実際の移動速度を計算
		const float finalSpeed = enemy.GetEnemyData().moveSpeed * speedMultiplier;
		enemy.AddTranslate(dir * finalSpeed * dt);
	}
}