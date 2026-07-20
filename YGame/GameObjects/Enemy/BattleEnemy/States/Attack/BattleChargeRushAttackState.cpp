#include "BattleChargeRushAttackState.h"

void BattleChargeRushAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	isContactDamageActive_ = false;

	if (auto anim = enemy.GetAnimation()) {
		anim->StartColorAnimation({ 1, 1, 1, 1 }, { 1.0f, 0.5f, 0.0f, 1.0f }, 0.3f);
		anim->StartRelativeScaleAnimation({ 1, 1, 1 }, { 1.2f, 0.7f, 1.2f }, enemy.GetEnemyData().attackParams.chargeRush.anticipationTime);
	}
	else {
		enemy.SetColor({ 1, 0.8f, 0.0f, 1 });
	}

	startY_ = enemy.GetTranslate().y;

	if (enemy.GetPlayer()) {
		attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(attackDir_) > 0.01f) {
			attackDir_ = Normalize(attackDir_);
			enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
		}
	}
}

void BattleChargeRushAttackState::Update(BattleEnemy& enemy, float dt) {
	const float currentTime = enemy.GetStateTimer();
	const float previousTime = currentTime - dt;
	const auto& params = enemy.GetEnemyData().attackParams.chargeRush;

	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float rushEndTime = chargeEndTime + params.rushTime;
	const float totalDuration = rushEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作 ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;
		const float sinkProgress = std::sin(progress * 3.14159f);
		Vector3 pos = enemy.GetTranslate();
		pos.y = startY_ - (params.stompIntensity * sinkProgress);
		enemy.SetTranslate(pos);

		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}
	}
	// === フェーズ2: チャージ ===
	else if (currentTime < chargeEndTime) {
		Vector3 pos = enemy.GetTranslate();
		pos.y = startY_;
		enemy.SetTranslate(pos);

		if (previousTime < anticipationEndTime && currentTime >= anticipationEndTime) {
			if (auto anim = enemy.GetAnimation()) {
				anim->PlayShakeAnimation(0.2f, params.chargeTime); // チャージ中震える
			}
		}

		if (enemy.GetPlayer()) {
			attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(attackDir_) > 0.01f) {
				attackDir_ = Normalize(attackDir_);
				enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
			}
		}

		if (!enemy.GetAnimation()) {
			const float chargeTime = currentTime - anticipationEndTime;
			const float blink = std::sin(chargeTime * 10.0f) * 0.3f + 0.7f;
			enemy.SetColor({ 1, 0.5f * blink, 0.0f, 1 });
		}
	}
	// === フェーズ3: 高速突進 ===
	else if (currentTime < rushEndTime) {
		isContactDamageActive_ = true;
		if (previousTime < chargeEndTime && currentTime >= chargeEndTime) {
			if (auto anim = enemy.GetAnimation()) {
				anim->StopAll(); // シェイク停止
				const Vector3 base = anim->GetBaseScale();
				anim->StartScaleAnimation(anim->GetCurrentScale(),
					{ base.x * 0.7f, base.y * 0.7f, base.z * 1.5f }, 0.1f);
			}
		}
		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
		enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * params.speedMultiplier * dt);
	}
	// === フェーズ4: クールダウン ===
	else if (currentTime >= totalDuration) {
		isContactDamageActive_ = false;
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
	else {
		isContactDamageActive_ = false;
	}
}

void BattleChargeRushAttackState::Exit(BattleEnemy& enemy) {
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

	Vector3 pos = enemy.GetTranslate();
	pos.y = startY_;
	enemy.SetTranslate(pos);
}
