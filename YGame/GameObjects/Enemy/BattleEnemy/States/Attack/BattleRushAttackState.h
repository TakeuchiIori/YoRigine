#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

/// <summary>
/// 攻撃状態
/// </summary>
class BattleRushAttackState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;

  const char *GetName() const override { return "Attack:Rush"; }
  bool CanBeParried() const override { return parriable_; }
  bool IsAttacking() const override { return true; }
  bool IsContactDamageActive() const override { return isContactDamageActive_; }

private:
  // Enter で攻撃データから読む（盾で受け止められる攻撃か）
  bool parriable_ = false;

  Vector3 attackDir_{0, 0, 0};
  Vector3 anticipationStartPos_{0, 0, 0};
  bool dirLocked_ = false;
  bool isContactDamageActive_ = false;
};
