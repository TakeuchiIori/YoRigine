#pragma once
#include "../../IEnemyState.h"
#include "../BattleEnemy.h"

/// <summary>
/// 回復/反撃準備状態（スーパーアーマー付き）
/// 一定回数攻撃を受けた後、この状態に移行して反撃チャンスを作る
/// </summary>
class BattleRecoveryState : public IEnemyState<BattleEnemy> {
public:
    void Enter(BattleEnemy& enemy) override;
    void Update(BattleEnemy& enemy, float dt) override;
    void Exit(BattleEnemy& enemy) override;

private:
    float recoveryDuration_ = 1.5f;  // 回復時間
    bool canCounter_ = false;        // カウンター可能フラグ
	bool hasPlayedAnimation_ = false; // アニメーション再生フラグ
};