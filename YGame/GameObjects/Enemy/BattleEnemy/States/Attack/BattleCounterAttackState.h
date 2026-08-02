#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"
#include "../BattleIdleState.h"
#include <cmath>

/// <summary>
/// 反撃状態（最強性能版）
/// RecoveryState終了後に移行する専用攻撃State
/// 起動フェーズ中は無敵、攻撃フェーズで無敵解除
/// プレイヤーを逃がさない超強化パラメータを使用
/// </summary>
class BattleCounterAttackState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;
  const char *GetName() const override { return "Attack:Counter"; }
  bool CanBeParried() const override { return parriable_; }

  bool IsAttacking() const override { return true; }
  bool IsContactDamageActive() const override { return isContactDamageActive_; }

private:
  // Enter で攻撃データから読む（盾で受け止められる攻撃か）
  bool parriable_ = false;

  Vector3 attackDir_{0, 0, 0};
  Vector3 anticipationStartPos_{0, 0, 0};
  bool isContactDamageActive_ = false;

  // ※ パラメータはすべて enemy.GetEnemyData().attackParams.counter から取得する
  //   (Phase: startup → anticipation → charge → rush → cooldown)
};
