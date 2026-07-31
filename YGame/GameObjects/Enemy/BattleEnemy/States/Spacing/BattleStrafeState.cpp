#include "BattleStrafeState.h"

#include "SpacingSelector.h"

#include <algorithm>

void BattleStrafeState::Enter(BattleEnemy &enemy) {
  enemy.ResetStateTimer();

  const SpacingParams &params = enemy.GetEnemyData().spacing;
  duration_ = SpacingSelector::RandomRange(params.strafeMinDuration,
                                           params.strafeMaxDuration);

  // 回り込む向きを毎回抽選する。固定すると同じ弧を描き続けて読まれやすい
  turnSign_ = (SpacingSelector::RandomRange(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
}

void BattleStrafeState::Update(BattleEnemy &enemy, float dt) {
  if (!enemy.GetPlayer()) {
    enemy.ChangeState(SpacingSelector::SelectReengage(enemy));
    return;
  }

  const SpacingParams &params = enemy.GetEnemyData().spacing;
  const float moveSpeed = enemy.GetEnemyData().moveSpeed;

  // プレイヤーから自分へ向かうベクトル（＝円の半径方向）
  Vector3 radial = enemy.GetTranslate() - enemy.GetPlayerPosition();
  radial.y = 0.0f;
  const float distance = Length(radial);
  if (distance < 0.01f) {
    // 完全に重なっているときは方向が決められないので、そのまま再交戦へ抜ける
    enemy.ChangeState(SpacingSelector::SelectReengage(enemy));
    return;
  }
  radial = Normalize(radial);

  // 半径方向に直交する円周方向。turnSign_ で左右どちらに回るかを決める
  const Vector3 tangent = {radial.z * turnSign_, 0.0f, -radial.x * turnSign_};

  // 維持したい間合いとのズレを速度で補正する。
  // distanceError > 0（遠すぎ）なら -radial ＝ プレイヤー方向へ寄る
  const float distanceError = distance - params.preferredDistance;
  const float correctionSpeed = std::clamp(
      distanceError * params.strafeDistanceKeepStrength, -moveSpeed, moveSpeed);

  const Vector3 velocity = tangent * moveSpeed * params.strafeSpeedMultiplier -
                           radial * correctionSpeed;
  enemy.AddTranslate(velocity * dt);

  // 横に動きながらもプレイヤーを正面に捉え続ける
  enemy.RotateTowardsPlayer(params.faceRotationSpeed, dt);

  if (enemy.GetStateTimer() >= duration_) {
    enemy.ChangeState(SpacingSelector::SelectReengage(enemy));
  }
}

void BattleStrafeState::Exit([[maybe_unused]] BattleEnemy &enemy) {}
