#include "BattleBackstepState.h"

#include "SpacingSelector.h"

#include <cmath>

void BattleBackstepState::Enter(BattleEnemy &enemy) {
  enemy.ResetStateTimer();

  // プレイヤーの反対方向へ下がる。Enter
  // で固定して、下がりながら向きが揺れないようにする
  retreatDir_ = enemy.GetTranslate() - enemy.GetPlayerPosition();
  retreatDir_.y = 0.0f;
  if (Length(retreatDir_) < 0.01f) {
    // 完全に重なっている場合は自分の背後へ下がる
    const float yaw = enemy.GetRotationY();
    retreatDir_ = {-std::sinf(yaw), 0.0f, -std::cosf(yaw)};
  }
  retreatDir_ = Normalize(retreatDir_);

  if (auto *anim = enemy.GetAnimation()) {
    // わずかに反り返らせて「引く」印象を出す
    anim->StartRelativeScaleAnimation(
        {1.0f, 1.0f, 1.0f}, {0.95f, 1.05f, 0.95f},
        enemy.GetEnemyData().spacing.backstepDuration,
        Easing::Function::EaseOutQuad);
  }
}

void BattleBackstepState::Update(BattleEnemy &enemy, float dt) {
  const SpacingParams &params = enemy.GetEnemyData().spacing;

  // 下がりながらプレイヤーを見続ける（背中を向けない）
  enemy.RotateTowardsPlayer(params.faceRotationSpeed, dt);

  // 後退は序盤ほど速く、終わりにかけて減速させる
  const float progress =
      std::fminf(enemy.GetStateTimer() / params.backstepDuration, 1.0f);
  const float speedScale = 1.0f - progress * progress;
  enemy.AddTranslate(retreatDir_ * enemy.GetEnemyData().moveSpeed *
                     params.backstepSpeedMultiplier * speedScale * dt);

  // 下がって間合いが戻ったので、横移動か様子見へ繋ぐ
  if (enemy.GetStateTimer() >= params.backstepDuration) {
    enemy.ChangeState(SpacingSelector::SelectAfterBackstep(enemy));
  }
}

void BattleBackstepState::Exit(BattleEnemy &enemy) {
  if (auto *anim = enemy.GetAnimation()) {
    anim->StartScaleAnimation(anim->GetCurrentScale(), anim->GetBaseScale(),
                              0.15f);
  }
}
