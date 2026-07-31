#include "BattleCounterAttackState.h"
#include "../Spacing/SpacingSelector.h"
#include <cmath>

void BattleCounterAttackState::Enter(BattleEnemy &enemy) {
  enemy.SetCanAct(false);
  enemy.ResetStateTimer();
  enemy.IsInvincible() = true;
  isContactDamageActive_ = false;

  anticipationStartPos_ = enemy.GetTranslate();

  if (enemy.GetPlayer()) {
    attackDir_ = enemy.GetPlayerPosition() - enemy.GetTranslate();
    if (Length(attackDir_) > 0.01f) {
      attackDir_ = Normalize(attackDir_);
      enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
    }
  }

  if (auto anim = enemy.GetAnimation()) {
    const float startupTime =
        enemy.GetEnemyData().attackParams.counter.startupTime;
    anim->StartColorAnimation({0.3f, 0.7f, 2.0f, 1.0f},
                              {1.0f, 0.1f, 0.0f, 1.0f}, startupTime,
                              Easing::Function::EaseInQuad);
    anim->PlayBounceScaleAnimation(1.3f, 0.85f);
  }
}

void BattleCounterAttackState::Update(BattleEnemy &enemy, float dt) {
  isContactDamageActive_ = false;
  const auto &p = enemy.GetEnemyData().attackParams.counter;
  const float currentTime = enemy.GetStateTimer();

  const float startupEnd = p.startupTime;
  const float anticipationEnd = startupEnd + p.anticipationTime;
  const float chargeEnd = anticipationEnd + p.chargeTime;
  const float rushEnd = chargeEnd + p.rushTime;
  const float totalDuration = rushEnd + p.cooldownTime;

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
    const float progress =
        (p.anticipationTime > 0.0f) ? localTime / p.anticipationTime : 1.0f;
    const float easeProgress = 1.0f - std::pow(1.0f - progress, 3.0f);
    const Vector3 backwardOffset =
        -attackDir_ * p.anticipationDistance * easeProgress;
    enemy.SetTranslate(anticipationStartPos_ + backwardOffset);
  } else if (currentTime < chargeEnd) {
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
    if (!enemy.GetAnimation() && p.chargeTime > 0.0f) {
      const float chargeProgress =
          (currentTime - anticipationEnd) / p.chargeTime;
      enemy.SetColor({1.0f, chargeProgress * 0.15f, 0.0f, 1.0f});
    }
  } else if (currentTime < rushEnd) {
    isContactDamageActive_ = true;
    enemy.SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    if (enemy.GetPlayer()) {
      Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
      if (Length(toPlayer) > 0.01f) {
        Vector3 newDir = Normalize(toPlayer);
        attackDir_ = Normalize(attackDir_ + newDir * p.rushHomingStrength * dt);
        enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
      }
    }
    enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed *
                       p.rushSpeedMultiplier * dt);
  } else if (currentTime >= totalDuration) {
    enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
  }
}

void BattleCounterAttackState::Exit(BattleEnemy &enemy) {
  isContactDamageActive_ = false;
  enemy.SetCanAct(true);
  enemy.IsInvincible() = false;

  if (auto anim = enemy.GetAnimation()) {
    anim->StopAll();
    anim->StartScaleAnimation(anim->GetCurrentScale(), anim->GetBaseScale(),
                              0.2f);
    anim->StartColorAnimation(anim->GetCurrentColor(), {1.0f, 1.0f, 1.0f, 1.0f},
                              0.2f);
  } else {
    enemy.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
  }
}
