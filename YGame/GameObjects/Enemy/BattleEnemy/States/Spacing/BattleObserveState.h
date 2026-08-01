#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

/// <summary>
/// その場で構えてプレイヤーを見る状態。
/// 移動も攻撃もしない「間」を作るためだけの状態で、
/// これがあるかないかで戦闘のリズムが大きく変わる。
/// </summary>
class BattleObserveState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;
  const char *GetName() const override { return "Spacing:Observe"; }

private:
  // Enter でランダムに決める様子見の長さ（秒）
  float duration_ = 0.8f;
};
