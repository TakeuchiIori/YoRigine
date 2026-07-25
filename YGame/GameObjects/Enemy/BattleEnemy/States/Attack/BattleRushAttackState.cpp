#include "BattleRushAttackState.h"
#include "../BattleIdleState.h"
#include <cmath>

void BattleRushAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	dirLocked_ = false;
	isContactDamageActive_ = false;

	if (auto anim = enemy.GetAnimation()) {
		anim->StartColorAnimation({ 1, 1, 1, 1 }, { 1.0f, 1.0f, 0.0f, 1.0f }, 0.2f);
		anim->StartRelativeScaleAnimation({ 1, 1, 1 }, { 1.1f, 0.9f, 0.7f }, enemy.GetEnemyData().attackParams.rush.anticipationTime, Easing::Function::EaseOutQuad);
	}
	else {
		enemy.SetColor({ 1, 1, 0.0f, 1 });
	}

	anticipationStartPos_ = enemy.GetTranslate();

	if (enemy.GetPlayer()) {
		attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(attackDir_) > 0.01f) {
			attackDir_ = Normalize(attackDir_);
			enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
		}
	}
}

void BattleRushAttackState::Update(BattleEnemy& enemy, float dt) {
	isContactDamageActive_ = false;
	const float currentTime = enemy.GetStateTimer();
	const float previousTime = currentTime - dt;
	const auto& params = enemy.GetEnemyData().attackParams.rush;

	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float rushEndTime = chargeEndTime + params.rushTime;
	const float totalDuration = rushEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作 ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;
		const float easeProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
		const Vector3 backwardOffset = -attackDir_ * params.anticipationDistance * easeProgress;
		enemy.SetTranslate(anticipationStartPos_ + backwardOffset);
	}
	// === フェーズ2: チャージ ===
	else if (currentTime < chargeEndTime) {
		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}
		if (!enemy.GetAnimation()) {
			const float chargeProgress = (currentTime - anticipationEndTime) / params.chargeTime;
			const float redIntensity = 0.5f + 0.5f * chargeProgress;
			enemy.SetColor({ 1, redIntensity * 0.3f, 0.0f, 1 });
		}
	}
	// === フェーズ3: 突進 ===
	else if (currentTime < rushEndTime) {
		isContactDamageActive_ = true;
		if (previousTime < chargeEndTime && currentTime >= chargeEndTime) {
			if (auto anim = enemy.GetAnimation()) {
				// 突進時に細長くなる（base 比 0.8/0.8/1.3 倍）
				const Vector3 base = anim->GetBaseScale();
				anim->StartScaleAnimation(anim->GetCurrentScale(),
					{ base.x * 0.8f, base.y * 0.8f, base.z * 1.3f }, 0.1f);
			}
		}
		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
		enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * params.speedMultiplier * dt);
	}
	// === フェーズ4: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleRushAttackState::Exit(BattleEnemy& enemy) {
	isContactDamageActive_ = false;
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
