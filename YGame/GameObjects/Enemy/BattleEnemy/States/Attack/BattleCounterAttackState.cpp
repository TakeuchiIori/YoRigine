#include "BattleCounterAttackState.h"
#include "../BattleIdleState.h"
#include <cmath>

void BattleCounterAttackState::Enter(BattleEnemy& enemy) {
    enemy.SetCanAct(false);
    enemy.ResetStateTimer();
    enemy.IsInvincible() = true;

    anticipationStartPos_ = enemy.GetTranslate();

    if (enemy.GetPlayer()) {
        attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
        if (Length(attackDir_) > 0.01f) {
            attackDir_ = Normalize(attackDir_);
            enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
        }
    }

    if (auto anim = enemy.GetAnimation()) {
        anim->StartColorAnimation(
            { 0.3f, 0.7f, 2.0f, 1.0f },
            { 1.0f, 0.1f, 0.0f, 1.0f },
            counterStartupTime_,
            Easing::Function::EaseInQuad
        );
        anim->PlayBounceScaleAnimation(1.3f, 0.85f);
    }
}

void BattleCounterAttackState::Update(BattleEnemy& enemy, float dt) {
    const float currentTime = enemy.GetStateTimer();

    const float startupEnd = counterStartupTime_;
    const float anticipationEnd = startupEnd + anticipationTime_;
    const float chargeEnd = anticipationEnd + chargeTime_;
    const float rushEnd = chargeEnd + rushTime_;
    const float totalDuration = rushEnd + cooldownTime_;

    if (currentTime < startupEnd) {
        if (enemy.GetPlayer()) {
            attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
            if (Length(attackDir_) > 0.01f) {
                attackDir_ = Normalize(attackDir_);
                enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
            }
        }
        return;
    }

    if (currentTime < anticipationEnd) {
        const float localTime = currentTime - startupEnd;
        const float progress = localTime / anticipationTime_;
        const float easeProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
        const Vector3 backwardOffset = -attackDir_ * anticipationDistance_ * easeProgress;
        enemy.SetTranslate(anticipationStartPos_ + backwardOffset);
    }
    else if (currentTime < chargeEnd) {
        if (enemy.IsInvincible()) {
            enemy.IsInvincible() = false;
        }
        if (enemy.GetPlayer()) {
            attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
            if (Length(attackDir_) > 0.01f) {
                attackDir_ = Normalize(attackDir_);
                enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
            }
        }
        if (!enemy.GetAnimation()) {
            const float chargeProgress = (currentTime - anticipationEnd) / chargeTime_;
            enemy.SetColor({ 1.0f, chargeProgress * 0.15f, 0.0f, 1.0f });
        }
    }
    else if (currentTime < rushEnd) {
        enemy.SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
        if (enemy.GetPlayer()) {
            Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
            if (Length(toPlayer) > 0.01f) {
                Vector3 newDir = Normalize(toPlayer);
                attackDir_ = Normalize(attackDir_ + newDir * 4.0f * dt);
                enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
            }
        }
        enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * rushSpeedMultiplier_ * dt);
    }
    else if (currentTime >= totalDuration) {
        enemy.ChangeState(std::make_unique<BattleIdleState>());
    }
}

void BattleCounterAttackState::Exit(BattleEnemy& enemy) {
    enemy.SetCanAct(true);
    enemy.IsInvincible() = false;

    if (auto anim = enemy.GetAnimation()) {
        anim->StopAll();
        anim->StartScaleAnimation(anim->GetCurrentScale(), anim->GetBaseScale(), 0.2f);
        anim->StartColorAnimation(anim->GetCurrentColor(), { 1.0f, 1.0f, 1.0f, 1.0f }, 0.2f);
    }
    else {
        enemy.SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
}