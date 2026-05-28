#include "BattleJumpAttackState.h"

void BattleJumpAttackState::Enter(BattleEnemy& enemy) {
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();

	if (auto anim = enemy.GetAnimation()) {
		// ジャンプ前はシアン色に変化しつつ、横に平べったく（縦に潰れる）なる
		anim->StartColorAnimation({ 1, 1, 1, 1 }, { 0.0f, 1.0f, 1.0f, 1.0f }, 0.3f);
		anim->StartScaleAnimation({ 1, 1, 1 }, { 1.3f, 0.6f, 1.3f }, 0.3f, Easing::Function::EaseOutQuad);
	}
	else {
		enemy.SetColor({ 0.0f, 1, 1, 1 });
	}

	startPos_ = enemy.GetTranslate();
	startY_ = startPos_.y;

	// ターゲット位置を決定
	if (enemy.GetPlayer()) {
		targetPos_ = enemy.GetPlayerPosition();
		Vector3 dir = targetPos_ - startPos_;
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
		}
	}
	else {
		targetPos_ = startPos_;
	}
}

void BattleJumpAttackState::Update(BattleEnemy& enemy, float dt) {
	const auto& params = enemy.GetEnemyData().attackParams.jump;
	const float currentTime = enemy.GetStateTimer();
	const float previousTime = currentTime - dt;

	// フェーズの境界時間を計算
	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float jumpEndTime = chargeEndTime + params.jumpTime;
	const float totalDuration = jumpEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作（深くしゃがむ） ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;

		// しゃがみ込む動き（イージング）
		const float crouchProgress = std::sin(progress * 1.5708f); // 0→1の曲線（π/2まで）

		Vector3 pos = startPos_;
		pos.y = std::max(0.0f, startY_ - (params.anticipationCrouchDepth * crouchProgress));
		enemy.SetTranslate(pos);

		// 色を点滅させて警告
		if (!enemy.GetAnimation()) {
			const float colorPulse = 0.5f + 0.5f * std::sin(currentTime * params.anticipationColorPulseSpeed);
			enemy.SetColor({ colorPulse, 1, 1, 1 });
		}
	}
	// === フェーズ2: チャージ（溜め） ===
	else if (currentTime < chargeEndTime) {
		const float chargeTime = currentTime - anticipationEndTime;
		const float chargeProgress = chargeTime / params.chargeTime;

		// さらに沈み込む
		Vector3 pos = startPos_;
		pos.y = std::max(0.0f, startY_ - params.anticipationCrouchDepth - (params.crouchDepth * chargeProgress));
		enemy.SetTranslate(pos);

		// 色を変化させる
		if (!enemy.GetAnimation()) {
			const float intensity = 0.5f + 0.5f * std::sin(chargeTime * 12.0f);
			enemy.SetColor({ intensity, 1, 0.0f, 1 });
		}
	}
	// === フェーズ3: ジャンプ ===
	else if (currentTime < jumpEndTime) {
		// ジャンプ開始の瞬間に縦長に引き伸ばすアニメーション
		if (previousTime < chargeEndTime && currentTime >= chargeEndTime) {
			if (auto anim = enemy.GetAnimation()) {
				anim->StartScaleAnimation({ 1.3f, 0.6f, 1.3f }, { 0.8f, 1.4f, 0.8f }, 0.15f, Easing::Function::EaseOutQuad);
			}
		}

		const float jumpTime = currentTime - chargeEndTime;
		const float jumpProgress = jumpTime / params.jumpTime;

		// 水平方向の補間 (Lerp)
		Vector3 pos;
		pos.x = startPos_.x + (targetPos_.x - startPos_.x) * jumpProgress;
		pos.z = startPos_.z + (targetPos_.z - startPos_.z) * jumpProgress;

		// 垂直方向の計算（放物線）
		const float heightOffset = params.jumpHeight * (4.0f * jumpProgress * (1.0f - jumpProgress));
		pos.y = std::max(0.0f, startY_ + heightOffset);

		enemy.SetTranslate(pos);

		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
	}
	// === フェーズ4: 着地（クールダウン） ===
	else if (currentTime < totalDuration) {
		// 着地した瞬間に少し潰れるアニメーション（バウンド感）
		if (previousTime < jumpEndTime && currentTime >= jumpEndTime) {
			if (auto anim = enemy.GetAnimation()) {
				anim->StartScaleAnimation({ 0.8f, 1.4f, 0.8f }, { 1.2f, 0.8f, 1.2f }, 0.1f, Easing::Function::EaseOutQuad, [enemyPtr = &enemy]() {
					if (auto a = enemyPtr->GetAnimation()) {
						a->StartScaleAnimation({ 1.2f, 0.8f, 1.2f }, { 1.0f, 1.0f, 1.0f }, 0.2f);
					}
					});
			}
		}

		// 着地位置で硬直
		Vector3 pos = targetPos_;
		pos.y = std::max(0.0f, startY_);
		enemy.SetTranslate(pos);

		// 徐々に色を戻す
		if (!enemy.GetAnimation()) {
			const float recoveryTime = currentTime - jumpEndTime;
			const float recoveryProgress = recoveryTime / params.cooldownTime;
			const float colorRecover = 1.0f - (1.0f - recoveryProgress) * 0.5f;
			enemy.SetColor({ 1, colorRecover, colorRecover, 1 });
		}
	}
	else {
		// 状態遷移
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleJumpAttackState::Exit(BattleEnemy& enemy) {
	// 元の高さに戻す
	Vector3 pos = enemy.GetTranslate();
	pos.y = startY_;
	enemy.SetTranslate(pos);

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