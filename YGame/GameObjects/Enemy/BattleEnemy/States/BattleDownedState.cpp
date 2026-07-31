#include "BattleDownedState.h"
#include "Spacing/SpacingSelector.h"

/// <summary>
/// ダウン状態開始時の処理
/// </summary>
void BattleDownedState::Enter(BattleEnemy &enemy) {
  enemy.SetCanAct(false);
  enemy.ResetStateTimer();
  enemy.SetColor({1.0f, 1.0f, 0.0f, 1.0f});
  enemy.IsDamageBlinking() = true;
  enemy.GetWT().anchorPoint_ = Vector3{0.0f, 0.0f, 0.0f};
}

/// <summary>
/// ダウン中の更新処理（揺れ演出）
/// </summary>
void BattleDownedState::Update(BattleEnemy &enemy, [[maybe_unused]] float dt) {
  float t = enemy.GetStateTimer();
  float angle = t * speed_;
  // sin/cos を別軸に使うと (rotX, rotZ)
  // が円を描きアンカー補正で位置が円軌道になる。 両軸とも sin
  // にしつつ周期を非整数比でずらすことで、円にならない自然なふらつきにする。
  float rotX = std::sin(angle * 1.3f) * tilt_ * 0.5f;
  float rotZ = std::sin(angle) * tilt_;

  enemy.GetRotationX() = rotX;
  enemy.GetRotationZ() = rotZ;

  // 一定時間経過で立ち上がり
  if (t > standUpTime_) {
    enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
  }
}

/// <summary>
/// ダウン終了時の処理
/// </summary>
void BattleDownedState::Exit(BattleEnemy &enemy) {
  enemy.SetCanAct(true);
  enemy.IsDamageBlinking() = false;
  enemy.SetColor({1, 1, 1, 1});
  enemy.GetWT().anchorPoint_ = Vector3{0.0f, 0.0f, 0.0f};
  enemy.GetRotationX() = 0.0f;
  enemy.GetRotationZ() = 0.0f;
}
