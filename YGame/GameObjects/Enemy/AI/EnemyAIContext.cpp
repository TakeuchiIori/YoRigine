#include "EnemyAIContext.h"

#include "../BaseEnemy.h"
#include "Player/Player.h"

#include <algorithm>
#include <cmath>
#include <numbers>

EnemyAIContext EnemyAIContext::Capture(const BaseEnemy &enemy,
                                       float facingHalfAngleDeg) {
  EnemyAIContext ctx;
  ctx.self = &enemy;
  ctx.selfHpRatio = enemy.GetHPRatio();

  Player *player = enemy.GetPlayer();
  if (!player) {
    return ctx;
  }

  ctx.player = player;
  ctx.hasPlayer = true;
  ctx.distance = enemy.GetDistanceToPlayer();
  ctx.playerIsInvincible = player->IsInvincible();
  if (player->GetMaxHP() > 0) {
    ctx.playerHpRatio = static_cast<float>(player->GetHP()) /
                        static_cast<float>(player->GetMaxHP());
  }

  // ── プレイヤーがこちらを向いているか ──
  // オートホーミングがあっても「向き」だけは埋められないので、
  // 背後・側面を取れているかが一番信頼できる隙の判断材料になる。
  {
    Vector3 toEnemy = enemy.GetTranslate() - player->GetWorldPosition();
    toEnemy.y = 0.0f;
    if (Length(toEnemy) > 0.001f) {
      toEnemy = Normalize(toEnemy);
      const float yaw = player->GetRotate().y;
      const Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
      const float dot = std::clamp(Dot(forward, toEnemy), -1.0f, 1.0f);
      ctx.facingAngleDeg = std::acos(dot) * 180.0f / std::numbers::pi_v<float>;
      ctx.playerIsFacingMe = ctx.facingAngleDeg <= facingHalfAngleDeg;
    }
  }

  // ── ロックオン。狙われているのが自分か、別の敵か ──
  if (const PlayerCamera *camera = player->GetPlayerCamera()) {
    if (camera->IsLockOn()) {
      const BaseCollider *target = camera->GetLockedTarget();
      if (target && target == enemy.GetPrimaryCollider()) {
        ctx.playerLockedOnMe = true;
      } else if (target) {
        ctx.playerLockedOnOther = true;
      }
    }
  }

  // ── 移動状態 ──
  if (const PlayerMovement *movement = player->GetMovement()) {
    ctx.playerIsMoving = movement->IsMoving();
    ctx.playerIsRunning = movement->IsRunning();
  }

  PlayerCombat *combat = player->GetCombat();
  if (!combat) {
    return ctx;
  }

  ctx.playerIsAttacking = combat->IsAttacking();
  ctx.playerIsGuarding = combat->IsGuarding();
  ctx.playerIsDodging = combat->IsDodging();
  ctx.playerIsStaggered = combat->IsHit() || combat->IsStunned();

  // ── 攻撃リソース（CC）──
  // CC が尽きている間は攻撃を出せないので、踏み込んでも反撃が来ない。
  const int maxCc = combat->GetMaxCC();
  if (maxCc > 0) {
    ctx.playerCcRatio =
        static_cast<float>(combat->GetCurrentCC()) / static_cast<float>(maxCc);
  }
  ctx.playerIsOutOfCC = combat->GetCurrentCC() <= 0;

  // 攻撃モーションのどこにいるか。
  // ComboState::Recovery は enum に定義があるだけで一度も設定されないため、
  // 攻撃データのヒット判定フレームと経過時間から自前で求める。
  // ただしオートホーミングで空振りがほぼ起きないため、振り終わりは
  // 隙としては扱わず、モニタ表示のための情報に留める。
  if (ctx.playerIsAttacking) {
    if (const PlayerCombo *combo = combat->GetCombo()) {
      if (const AttackData *attack = combo->GetCurrentAttack()) {
        const float fps =
            (attack->fps > 0) ? static_cast<float>(attack->fps) : 60.0f;
        const float elapsed = combo->GetStateTimer();
        ctx.playerIsWindingUp =
            elapsed < (static_cast<float>(attack->hitStart) / fps);
        ctx.playerIsRecovering =
            elapsed > (static_cast<float>(attack->hitEnd) / fps);
      }
    }
  }

  return ctx;
}
