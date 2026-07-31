#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

/// <summary>
/// プレイヤーとの距離を保ったまま円周上を横移動する状態。
/// 「間合いを測っている」印象を作る。近すぎ／遠すぎは移動しながら補正する。
/// </summary>
class BattleStrafeState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;

private:
  // Enter でランダムに決める横移動の長さ（秒）
  float duration_ = 1.2f;
  // 回り込む向き（+1 = 右回り / -1 = 左回り）
  float turnSign_ = 1.0f;
};
