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
    void Enter(BattleEnemy& enemy) override;
    void Update(BattleEnemy& enemy, float dt) override;
    void Exit(BattleEnemy& enemy) override;

private:
    Vector3 attackDir_{ 0, 0, 0 };
    Vector3 anticipationStartPos_{ 0, 0, 0 };

    // ========== 反撃専用パラメータ ==========

    // フェーズ1: カウンター起動（この間は無敵）
    const float counterStartupTime_ = 0.2f;

    // フェーズ2: 予備動作（短くキビキビ）
    const float anticipationTime_ = 0.5f;
    const float anticipationDistance_ = 10.8f;   // 後退距離）

    // フェーズ3: チャージ（プレイヤーをロックオン追尾）
    const float chargeTime_ = 0.25f;

    // フェーズ4: 突進（超高速 & 突進中もプレイヤーを追従）
    const float rushTime_ = 0.55f;
    const float rushSpeedMultiplier_ = 15.0f;

    // フェーズ5: クールダウン
    const float cooldownTime_ = 0.8f;
};