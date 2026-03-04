#include "BattleCounterAttackState.h"
#include "../BattleIdleState.h"
#include <cmath>

/// <summary>
/// 反撃状態開始処理
/// RecoveryStateから来るため、入った時点では無敵を維持する
/// </summary>
void BattleCounterAttackState::Enter(BattleEnemy& enemy) {
    enemy.SetCanAct(false);
    enemy.ResetStateTimer();
    enemy.IsInvincible() = true;  // 起動フェーズ中は無敵を維持

    anticipationStartPos_ = enemy.GetTranslate();

    // 入った瞬間にプレイヤー方向を確定
    if (enemy.GetPlayer()) {
        attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
        if (Length(attackDir_) > 0.01f) {
            attackDir_ = Normalize(attackDir_);
            enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
        }
    }

    // 青白くフラッシュ → 赤に変化する起動演出
    if (enemy.GetAnimation()) {
        enemy.GetAnimation()->StartColorAnimation(
            { 0.3f, 0.7f, 2.0f, 1.0f },  // 強い青白フラッシュ
            { 1.0f, 0.1f, 0.0f, 1.0f },  // 赤へ変化
            counterStartupTime_,
            Easing::Function::EaseInQuad
        );
        enemy.GetAnimation()->PlayBounceScaleAnimation(1.3f, 0.85f);
    }
}

/// <summary>
/// 反撃状態更新処理
/// </summary>
void BattleCounterAttackState::Update(BattleEnemy& enemy, float dt) {
    const float currentTime = enemy.GetStateTimer();

    // フェーズ境界時間
    const float startupEnd = counterStartupTime_;
    const float anticipationEnd = startupEnd + anticipationTime_;
    const float chargeEnd = anticipationEnd + chargeTime_;
    const float rushEnd = chargeEnd + rushTime_;
    const float totalDuration = rushEnd + cooldownTime_;

    // === フェーズ1: カウンター起動（無敵・演出のみ） ===
    if (currentTime < startupEnd) {
        // 起動中もプレイヤーを追尾して向く（逃がさない）
        if (enemy.GetPlayer()) {
            attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
            if (Length(attackDir_) > 0.01f) {
                attackDir_ = Normalize(attackDir_);
                enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
            }
        }
        return;
    }

    // === フェーズ2: 予備動作（素早く後退）※この間はまだ無敵 ===
    if (currentTime < anticipationEnd) {
        const float localTime = currentTime - startupEnd;
        const float progress = localTime / anticipationTime_;

        // ease-out cubicで素早くスパッと引く
        const float easeProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
        const Vector3 backwardOffset = -attackDir_ * anticipationDistance_ * easeProgress;
        enemy.SetTranslate(anticipationStartPos_ + backwardOffset);

    }
    // === フェーズ3: チャージ（予備動作完了 → ここで無敵解除、プレイヤーをロックオン追尾） ===
    else if (currentTime < chargeEnd) {
        // チャージ開始の瞬間に無敵解除（後退完了済みなので安全）
        if (enemy.IsInvincible()) {
            enemy.IsInvincible() = false;
        }
        // チャージ中もリアルタイムでプレイヤーを追尾して方向を更新
        if (enemy.GetPlayer()) {
            attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
            if (Length(attackDir_) > 0.01f) {
                attackDir_ = Normalize(attackDir_);
                enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
            }
        }

        const float chargeProgress = (currentTime - anticipationEnd) / chargeTime_;
        enemy.SetColor({ 1.0f, chargeProgress * 0.15f, 0.0f, 1.0f });
    }
    // === フェーズ4: 突進（超高速 & 突進中もプレイヤーを追従） ===
    else if (currentTime < rushEnd) {
        enemy.SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });

        // 突進中もプレイヤー方向を毎フレーム更新（ホーミング）
        if (enemy.GetPlayer()) {
            Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
            if (Length(toPlayer) > 0.01f) {
                // 完全ホーミングではなく少し補間して自然に追う
                Vector3 newDir = Normalize(toPlayer);
                attackDir_ = Normalize(attackDir_ + newDir * 4.0f * dt);
                enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
            }
        }

        enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * rushSpeedMultiplier_ * dt);
    }
    // === フェーズ5: クールダウン ===
    else if (currentTime >= totalDuration) {
        enemy.ChangeState(std::make_unique<BattleIdleState>());
    }
}

/// <summary>
/// 反撃状態終了処理
/// </summary>
void BattleCounterAttackState::Exit(BattleEnemy& enemy) {
    enemy.SetCanAct(true);
    enemy.IsInvincible() = false;  // 念のためリセット
    enemy.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}