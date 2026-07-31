#include "BattleDamageState.h"
#include "Spacing/SpacingSelector.h"

/// <summary>
/// ダメージ状態開始処理
/// </summary>
void BattleDamageState::Enter(BattleEnemy &enemy) {
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
        {1.5f, 1.5f, 1.5f, 1.0f}, // 一瞬明るく
        {1.0f, 0.2f, 0.2f, 1.0f}, // 赤く
        0.2f, Easing::Function::EaseOutQuad);
  }
}

/// <summary>
/// ダメージ状態更新処理
/// </summary>
void BattleDamageState::Update(BattleEnemy &enemy, float dt) {
  (void)dt; // dtは今のところ使用しない
  // ノックバック中の場合は待機
  if (enemy.GetKnockbackData().isKnockingBack_) {
    enemy.ResetStateTimer();
    return;
  }

  // 一定時間経過で交戦へ復帰する。
  // 以前は距離を無視して必ず Rush へ直行していたため、
  // 「殴られる→1秒後に必ず突進」の単調なループになっていた。
  if (enemy.GetStateTimer() > 1.0f) {
    enemy.ChangeState(SpacingSelector::SelectReengage(enemy));
  }
}

/// <summary>
/// ダメージ状態終了処理
/// </summary>
void BattleDamageState::Exit(BattleEnemy &enemy) {
  enemy.SetCanAct(true);
  enemy.IsDamageBlinking() = false;

  // 色を元に戻す
  if (enemy.GetAnimation()) {
    enemy.GetAnimation()->StartColorAnimation(
        enemy.GetAnimation()->GetCurrentColor(), {1.0f, 1.0f, 1.0f, 1.0f},
        0.15f, Easing::Function::Linear);
  }
}