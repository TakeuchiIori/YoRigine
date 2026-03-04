#include "BattleRecoveryState.h"

#include "Attack/BattleRushAttackState.h"
#include "BattleIdleState.h"

void BattleRecoveryState::Enter(BattleEnemy& enemy) {
    enemy.SetCanAct(false);
    enemy.IsInvincible() = true;  // 無敵状態にする
    enemy.ResetStateTimer();
    hasPlayedAnimation_ = false;

    // バウンススケールアニメーション（気合を溜めるような演出）
    if (enemy.GetAnimation()) {
        enemy.GetAnimation()->PlayBounceScaleAnimation(1.3f, 0.8f);
    }

    // カラーアニメーション（青白く光る）
    if (enemy.GetAnimation()) {
        enemy.GetAnimation()->StartColorAnimation(
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.7f, 0.9f, 1.2f, 1.0f },
            0.5f,
            Easing::Function::EaseInOutQuad
        );
    }
}

void BattleRecoveryState::Update(BattleEnemy& enemy, float dt) {
	(void)dt; // dtは今のところ使用しない
    float timer = enemy.GetStateTimer();

    // 回復時間の70%経過したら色を戻し始める
    if (timer > recoveryDuration_ * 0.7f && !hasPlayedAnimation_) {
        hasPlayedAnimation_ = true;
        if (enemy.GetAnimation()) {
            enemy.GetAnimation()->StartColorAnimation(
                enemy.GetAnimation()->GetCurrentColor(),
                { 1.0f, 1.0f, 1.0f, 1.0f },
                0.3f,
                Easing::Function::EaseOutQuad
            );
        }
    }

    // 回復時間終了で攻撃状態へ
    if (timer > recoveryDuration_) {
        // 反撃
        enemy.ChangeState(std::make_unique<BattleRushAttackState>());
    }
}

void BattleRecoveryState::Exit(BattleEnemy& enemy) {
    enemy.SetCanAct(true);
    enemy.IsInvincible() = false;

    // 念のため色をリセット
    enemy.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}