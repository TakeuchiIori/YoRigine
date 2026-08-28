#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"
#include <random>

/// <summary>
/// ダウン状態
/// </summary>
class BattleDownedState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;
  bool KeepsStateWhenDamaged() const override { return true; }
  const char *GetName() const override { return "Downed"; }

  // ふらつきの速さ・傾き・立ち上がり時間は
  // enemyData_.damageReaction から取るのでメンバは持たない。
};
