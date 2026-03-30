#include "BattleSpinAttackState.h"

void BattleSpinAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();

	if (auto anim = enemy.GetAnimation()) {
		anim->StartColorAnimation({ 1, 1, 1, 1 }, { 1.0f, 0.5f, 1.0f, 1.0f }, 0.3f);
		anim->StartScaleAnimation({ 1, 1, 1 }, { 1.3f, 0.8f, 1.3f }, enemy.GetEnemyData().attackParams.spin.anticipationTime);
	}
	else {
		enemy.SetColor({ 1, 0.5f, 1, 1 });
	}

	startRotation_ = enemy.GetRotationY();

	if (enemy.GetPlayer()) {
		Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
			startRotation_ = enemy.GetRotationY();
		}
	}

	targetTwistRotation_ = startRotation_ - enemy.GetEnemyData().attackParams.spin.twistAngle;
}

void BattleSpinAttackState::Update(BattleEnemy& enemy, float dt) {
	const float currentTime = enemy.GetStateTimer();
	const auto& params = enemy.GetEnemyData().attackParams.spin;
	const float PI = std::numbers::pi_v<float>;

	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float spinEndTime = chargeEndTime + params.spinTime;
	const float totalDuration = spinEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作 ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;
		const float easeProgress = progress < 0.5f
			? 2.0f * progress * progress
			: 1.0f - std::pow(-2.0f * progress + 2.0f, 2.0f) / 2.0f;

		const float currentRotation = startRotation_ + (targetTwistRotation_ - startRotation_) * easeProgress;
		enemy.SetRotationY(currentRotation);
	}
	// === フェーズ2: チャージ ===
	else if (currentTime < chargeEndTime) {
		enemy.SetRotationY(targetTwistRotation_);
		if (!enemy.GetAnimation()) {
			const float chargeTime = currentTime - anticipationEndTime;
			const float pulse = 0.5f + 0.5f * std::sin(chargeTime * 15.0f);
			enemy.SetColor({ 1, 0.2f + pulse * 0.3f, 1, 1 });
		}
	}
	// === フェーズ3: 回転攻撃 ===
	else if (currentTime < spinEndTime) {
		const float spinTime = currentTime - chargeEndTime;
		const float spinProgress = spinTime / params.spinTime;

		const float totalRotation = params.rotationCount * 2.0f * PI;
		enemy.SetRotationY(targetTwistRotation_ + spinProgress * totalRotation);

		if (enemy.GetPlayer()) {
			Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(dir) > 0.01f) {
				dir = Normalize(dir);
				enemy.AddTranslate(dir * enemy.GetEnemyData().moveSpeed * params.moveSpeedMultiplier * dt);
			}
		}

		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
	}
	// === フェーズ4: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleSpinAttackState::Exit(BattleEnemy& enemy) {
	if (enemy.GetPlayer()) {
		Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
		}
	}

	enemy.SetCanAct(true);

	if (auto anim = enemy.GetAnimation()) {
		anim->StopAll();
		anim->StartScaleAnimation(anim->GetCurrentScale(), { 1, 1, 1 }, 0.2f);
		anim->StartColorAnimation(anim->GetCurrentColor(), { 1, 1, 1, 1 }, 0.2f);
	}
	else {
		enemy.SetColor({ 1, 1, 1, 1 });
	}
}