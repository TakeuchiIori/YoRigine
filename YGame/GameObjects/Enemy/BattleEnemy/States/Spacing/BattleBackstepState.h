#pragma once
#include "../../../IEnemyState.h"
#include "../../BattleEnemy.h"

/// <summary>
/// 後退して間合いをリセットする状態。
/// プレイヤーの方を向いたまま下がるので「距離を取り直している」ように見える。
/// 終了後は Observe へ繋いで一拍置く。
/// </summary>
class BattleBackstepState : public IEnemyState<BattleEnemy> {
public:
  void Enter(BattleEnemy &enemy) override;
  void Update(BattleEnemy &enemy, float dt) override;
  void Exit(BattleEnemy &enemy) override;
  const char *GetName() const override { return "Spacing:Backstep"; }

private:
  // 後退方向（Enter でプレイヤーの反対方向に固定する）
  Vector3 retreatDir_{};
};
