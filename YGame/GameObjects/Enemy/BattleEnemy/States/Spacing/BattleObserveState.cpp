#include "BattleObserveState.h"

#include "SpacingSelector.h"

void BattleObserveState::Enter(BattleEnemy &enemy) {
  enemy.ResetStateTimer();

  const SpacingParams &params = enemy.GetEnemyData().spacing;
  duration_ = SpacingSelector::RandomRange(params.observeMinDuration,
                                           params.observeMaxDuration);
}

void BattleObserveState::Update(BattleEnemy &enemy, float dt) {
  // 移動せず、プレイヤーだけを見続ける。この「何もしない時間」が間になる
  enemy.RotateTowardsPlayer(enemy.GetEnemyData().spacing.faceRotationSpeed, dt);

  if (enemy.GetStateTimer() >= duration_) {
    enemy.ChangeState(SpacingSelector::SelectReengage(enemy));
  }
}

void BattleObserveState::Exit([[maybe_unused]] BattleEnemy &enemy) {}
