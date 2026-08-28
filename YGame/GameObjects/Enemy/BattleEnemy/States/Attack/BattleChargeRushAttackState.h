#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"
#include "../BattleIdleState.h"
#include <cmath>

/// <summary>
/// チャージ攻撃状態（長めに溜めて強力な突進）
/// </summary>
class BattleChargeRushAttackState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;

  void Update(BattleEnemy &enemy, float dt) override;

  void Exit(BattleEnemy &enemy) override;
  const char *GetName() const override { return "Attack:ChargeRush"; }
  bool CanBeParried() const override { return parriable_; }

  bool IsAttacking() const override { return true; }
  bool IsContactDamageActive() const override { return isContactDamageActive_; }

private:
  // Enter で攻撃データから読む（盾で受け止められる攻撃か）
  bool parriable_ = false;

  Vector3 attackDir_{0, 0, 0};
  float startY_ = 0.0f;
  bool isContactDamageActive_ = false;
};
