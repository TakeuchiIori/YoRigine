#include "BattleApproachState.h"
#include "Attack/BattleRushAttackState.h"
#include "BattleIdleState.h"

#include "Spacing/SpacingSelector.h"
/// <summary>
/// 接近状態更新処理
/// </summary>
void BattleApproachState::Update(BattleEnemy &enemy, float dt) {
  // プレイヤーがいない場合は待機状態へ
  if (!enemy.GetPlayer()) {
    enemy.ChangeState(std::make_unique<BattleIdleState>());
    return;
  }

  Vector3 pos = enemy.GetTranslate();
  Vector3 playerPos = enemy.GetPlayerPosition();
  Vector3 dir = playerPos - pos;
  float dist = Length(dir);

  // プレイヤーに向かって移動
  if (dist > 0.1f) {
    dir = Normalize(dir);
    enemy.AddTranslate(dir * enemy.GetEnemyData().moveSpeed * dt);
    enemy.SetRotationY(std::atan2(dir.x, dir.z));
  }

  // 一定距離以内で攻撃状態へ
  if (dist < enemy.GetEnemyData().attackStateRange) {
    // 攻撃権が取れなければ攻撃せず、周囲を回って順番を待つ
    enemy.ChangeState(SpacingSelector::AttackOrCircle(enemy));
  }
}
