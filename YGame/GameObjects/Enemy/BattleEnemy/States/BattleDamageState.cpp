#include "BattleDamageState.h"
#include "Spacing/SpacingSelector.h"

/// <summary>
/// ダメージ状態開始処理
/// </summary>
void BattleDamageState::Enter(BattleEnemy &enemy) {
  const DamageReactionParams &params = enemy.GetEnemyData().damageReaction;

  enemy.SetCanAct(false);
  enemy.IsDamageBlinking() = true;
  enemy.SetBlinkSpeed(params.blinkSpeed);
  enemy.ResetStateTimer();

  if (auto *anim = enemy.GetAnimation()) {
    // パンチアニメーション（ヒット時に少し拡大）
    anim->PlayPunchAnimation(params.punchScale, params.punchDuration);

    // カラーアニメーション（白→赤に点滅）
    anim->StartColorAnimation({1.5f, 1.5f, 1.5f, 1.0f}, // 一瞬明るく
                              {1.0f, 0.2f, 0.2f, 1.0f}, // 赤く
                              params.flashDuration,
                              Easing::Function::EaseOutQuad);
  }
}

/// <summary>
/// ダメージ状態更新処理
/// </summary>
void BattleDamageState::Update(BattleEnemy &enemy, float dt) {
  (void)dt; // dtは今のところ使用しない
  const DamageReactionParams &params = enemy.GetEnemyData().damageReaction;

  // ノックバック中は硬直タイマーを進めない。
  // この場合の実際の硬直は「ノックバック時間 + staggerDuration」になる。
  if (params.waitForKnockback && enemy.GetKnockbackData().isKnockingBack_) {
    enemy.ResetStateTimer();
    return;
  }

  // 一定時間経過で交戦へ復帰する。
  // 以前は距離を無視して必ず Rush へ直行していたため、
  // 「殴られる→1秒後に必ず突進」の単調なループになっていた。
  if (enemy.GetStateTimer() > params.staggerDuration) {
    enemy.ChangeState(SpacingSelector::SelectReengage(enemy));
  }
}

/// <summary>
/// ダメージ状態終了処理
/// </summary>
void BattleDamageState::Exit(BattleEnemy &enemy) {
  const DamageReactionParams &params = enemy.GetEnemyData().damageReaction;

  enemy.SetCanAct(true);
  enemy.IsDamageBlinking() = false;

  // 色を元に戻す
  if (auto *anim = enemy.GetAnimation()) {
    anim->StartColorAnimation(anim->GetCurrentColor(), {1.0f, 1.0f, 1.0f, 1.0f},
                              params.colorReturnDuration,
                              Easing::Function::Linear);
  }
}
