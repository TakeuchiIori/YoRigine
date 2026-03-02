#include "BattleSpinAttackState.h"

void BattleSpinAttackState::Enter(BattleEnemy& enemy)
{
	enemy.SetCanAct(false);
	enemy.ResetStateTimer();
	enemy.SetColor({ 1, 0.5f, 1, 1 }); // 紫色で予備動作を示す

	startRotation_ = enemy.GetRotationY();

	// プレイヤーの方を向く
	if (enemy.GetPlayer()) {
		Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
			startRotation_ = enemy.GetRotationY();
		}
	}

	// 捻り目標角度を設定（逆方向に捻る）
	targetTwistRotation_ = startRotation_ - enemy.GetEnemyData().attackParams.spin.twistAngle;
}

void BattleSpinAttackState::Update(BattleEnemy& enemy, float dt)
{
	const float currentTime = enemy.GetStateTimer();
	const auto& params = enemy.GetEnemyData().attackParams.spin;
	const float PI = 3.14159f;

	// フェーズの境界時間を計算
	const float anticipationEndTime = params.anticipationTime;
	const float chargeEndTime = anticipationEndTime + params.chargeTime;
	const float spinEndTime = chargeEndTime + params.spinTime;
	const float totalDuration = spinEndTime + params.cooldownTime;

	// === フェーズ1: 予備動作（体を捻る） ===
	if (currentTime < anticipationEndTime) {
		const float progress = currentTime / params.anticipationTime;

		// イージングを使って体を捻る（ease-in-out）
		const float easeProgress = progress < 0.5f
			? 2.0f * progress * progress
			: 1.0f - std::pow(-2.0f * progress + 2.0f, 2.0f) / 2.0f;

		const float currentRotation = startRotation_ + (targetTwistRotation_ - startRotation_) * easeProgress;
		enemy.SetRotationY(currentRotation);

		// 色の変化
		const float colorIntensity = params.anticipationColorIntensity + (1.0f - params.anticipationColorIntensity) * progress;
		enemy.SetColor({ 1, 0.5f * colorIntensity, 1, 1 });
	}
	// === フェーズ2: チャージ（短い溜め） ===
	else if (currentTime < chargeEndTime) {
		// 捻った状態を維持
		enemy.SetRotationY(targetTwistRotation_);

		// 色を点滅
		const float chargeTime = currentTime - anticipationEndTime;
		const float pulse = 0.5f + 0.5f * std::sin(chargeTime * 15.0f);
		enemy.SetColor({ 1, 0.2f + pulse * 0.3f, 1, 1 });
	}
	// === フェーズ3: 回転攻撃 ===
	else if (currentTime < spinEndTime) {
		const float spinTime = currentTime - chargeEndTime;
		const float spinProgress = spinTime / params.spinTime;

		// 回転する
		const float totalRotation = params.rotationCount * 2.0f * PI;
		enemy.SetRotationY(targetTwistRotation_ + spinProgress * totalRotation);

		// 回転しながら少し前進
		if (enemy.GetPlayer()) {
			Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
			if (Length(dir) > 0.01f) {
				dir = Normalize(dir);
				enemy.AddTranslate(dir * enemy.GetEnemyData().moveSpeed * params.moveSpeedMultiplier * dt);
			}
		}

		// 回転中は赤色
		enemy.SetColor({ 1, 0.0f, 0.0f, 1 });
	}
	// === フェーズ4: クールダウン ===
	else if (currentTime >= totalDuration) {
		enemy.ChangeState(std::make_unique<BattleIdleState>());
	}
}

void BattleSpinAttackState::Exit(BattleEnemy& enemy)
{
	// プレイヤーの方を向く
	if (enemy.GetPlayer()) {
		Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
		if (Length(dir) > 0.01f) {
			enemy.SetRotationY(std::atan2(dir.x, dir.z));
		}
	}

	enemy.SetCanAct(true);
	enemy.SetColor({ 1, 1, 1, 1 });
}