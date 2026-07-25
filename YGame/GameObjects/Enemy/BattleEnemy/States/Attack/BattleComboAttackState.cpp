#include "BattleComboAttackState.h"
#include "Player/Player.h"

void BattleComboAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();

	if (auto anim = enemy.GetAnimation()) {
		anim->StartColorAnimation({ 1, 1, 1, 1 }, { 1.0f, 0.0f, 1.0f, 1.0f }, 0.2f);
		anim->StartRelativeScaleAnimation({ 1, 1, 1 }, { 0.8f, 1.2f, 0.8f }, enemy.GetEnemyData().attackParams.combo.anticipationTime);
	}
	else {
		enemy.SetColor({ 1, 0.0f, 1, 1 });
	}

	comboCount_ = 0;
	contactDamageWindow_ = -1;
	hasPerformedAnticipation_ = false;
	anticipationStartPos_ = enemy.GetTranslate();
}

void BattleComboAttackState::Update(BattleEnemy& enemy, float dt) {
	contactDamageWindow_ = -1;
	const auto& params = enemy.GetEnemyData().attackParams.combo;
	const float currentTime = enemy.GetStateTimer();
	const int maxHits = 3;

	const float anticipationEndTime = params.anticipationTime;
	const float totalComboTime = anticipationEndTime + (params.phaseDuration * maxHits);
	const float totalDuration = totalComboTime + params.cooldownTime;

	// === フェーズ1: 予備動作 ===
	if (currentTime < anticipationEndTime && !hasPerformedAnticipation_) {
		const float progress = currentTime / params.anticipationTime;
		Player* player = enemy.GetPlayer();
		if (player) {
			Vector3 toPlayer = player->GetTranslate() - anticipationStartPos_;
			if (Length(toPlayer) > 0.01f) {
				Vector3 backwardDir = -Normalize(toPlayer);
				const float easeProgress = 1.0f - std::pow(1.0f - progress, 2.0f);
				const Vector3 backstepOffset = backwardDir * params.anticipationBackstepDistance * easeProgress;
				enemy.SetTranslate(anticipationStartPos_ + backstepOffset);
				enemy.SetRotationY(std::atan2(toPlayer.x, toPlayer.z));
			}
		}

		if (progress >= 0.99f) {
			hasPerformedAnticipation_ = true;
		}
	}
	// === フェーズ2: コンボ攻撃 ===
	else if (currentTime >= anticipationEndTime && currentTime < totalComboTime) {
		const float comboTime = currentTime - anticipationEndTime;
		const int currentHit = static_cast<int>(comboTime / params.phaseDuration);
		const float timeInPhase = std::fmod(comboTime, params.phaseDuration);
		const float previousTimeInPhase = timeInPhase - dt;
		comboCount_ = currentHit;

		const bool isSubCharging = (timeInPhase < params.subChargeTime);
		const bool isSubRushing = (!isSubCharging && timeInPhase < params.subChargeTime + params.subRushTime);

		if (isSubCharging) {
			UpdateOrientation(enemy);

			// 各ヒットの溜め開始時
			if (previousTimeInPhase < 0.0f && timeInPhase >= 0.0f) {
				if (auto anim = enemy.GetAnimation()) {
					anim->StartRelativeScaleAnimation({ 0.8f, 1.2f, 0.8f }, { 1.0f, 0.9f, 0.9f }, params.subChargeTime);
				}
			}

			if (!enemy.GetAnimation()) {
				const float chargeProgress = timeInPhase / params.subChargeTime;
				enemy.SetColor({ 1, 0.0f, 0.5f + 0.5f * chargeProgress, 1 });
			}
		}
		else if (isSubRushing) {
			// コンボの各突進を独立した1回の攻撃判定として扱う。
			contactDamageWindow_ = currentHit;
			// 各ヒットの突進開始時
			if (previousTimeInPhase < params.subChargeTime && timeInPhase >= params.subChargeTime) {
				if (auto anim = enemy.GetAnimation()) {
					anim->StartRelativeScaleAnimation({ 1.0f, 0.9f, 0.9f }, { 0.8f, 0.8f, 1.2f }, params.subRushTime);
				}
			}

			const float currentMultiplier = params.rushSpeedMultiplier + (comboCount_ * 1.0f);
			ExecuteRush(enemy, currentMultiplier, dt);
			enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
		}
	}
	// === フェーズ3: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleComboAttackState::Exit(BattleEnemy& enemy) {
	contactDamageWindow_ = -1;
	enemy.SetCanAct(true);
	if (auto anim = enemy.GetAnimation()) {
		anim->StopAll();
		anim->StartScaleAnimation(anim->GetCurrentScale(), anim->GetBaseScale(), 0.2f);
		anim->StartColorAnimation(anim->GetCurrentColor(), { 1, 1, 1, 1 }, 0.2f);
	}
	else {
		enemy.SetColor({ 1, 1, 1, 1 });
	}
}

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
	Vector3 toPlayer = player->GetTranslate() - enemy.GetTranslate();
	if (Length(toPlayer) > 0.01f) {
		Vector3 dir = Normalize(toPlayer);
		const float finalSpeed = enemy.GetEnemyData().moveSpeed * speedMultiplier;
		enemy.AddTranslate(dir * finalSpeed * dt);
	}
}
