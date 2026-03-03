#include "BattleDamageState.h"
#include "Attack/BattleRushAttackState.h"

/// <summary>
/// ダメージ状態開始処理
/// </summary>
void BattleDamageState::Enter(BattleEnemy& enemy) {
    enemy.SetCanAct(false);
    enemy.IsDamageBlinking() = true;
    enemy.ResetStateTimer();

    // パンチアニメーション（ヒット時に少し拡大）
    if (enemy.GetAnimation()) {
        enemy.GetAnimation()->PlayPunchAnimation(0.15f, 0.25f);
    }

    // カラーアニメーション（白→赤に点滅）
    if (enemy.GetAnimation()) {
        enemy.GetAnimation()->StartColorAnimation(
            { 1.5f, 1.5f, 1.5f, 1.0f }, // 一瞬明るく
            { 1.0f, 0.2f, 0.2f, 1.0f }, // 赤く
            0.2f,
            Easing::Function::EaseOutQuad
        );
    }
}

/// <summary>
/// ダメージ状態更新処理
/// </summary>
void BattleDamageState::Update(BattleEnemy& enemy, float dt) {
	(void)dt; // dtは今のところ使用しない
    // ノックバック中の場合は待機
    if (enemy.GetKnockbackData().isKnockingBack_) {
        enemy.ResetStateTimer();
        return;
    }

    // 一定時間経過で攻撃状態へ
    if (enemy.GetStateTimer() > 1.0f) {
        enemy.ChangeState(std::make_unique<BattleRushAttackState>());
    }
}

/// <summary>
/// ダメージ状態終了処理
/// </summary>
void BattleDamageState::Exit(BattleEnemy& enemy) {
    enemy.SetCanAct(true);
    enemy.IsDamageBlinking() = false;

    // 色を元に戻す
    if (enemy.GetAnimation()) {
        enemy.GetAnimation()->StartColorAnimation(
            enemy.GetAnimation()->GetCurrentColor(),
            { 1.0f, 1.0f, 1.0f, 1.0f },
            0.15f,
            Easing::Function::Linear
        );
    }
}