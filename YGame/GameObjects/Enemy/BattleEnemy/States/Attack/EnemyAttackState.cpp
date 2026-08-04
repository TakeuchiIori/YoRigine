#include "EnemyAttackState.h"

#include "../Spacing/SpacingSelector.h"
#include "Particle/EffectHandle.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// Scripted フェーズの登録表。攻撃データから名前で引く。
std::unordered_map<std::string, EnemyAttackState::ScriptedPhaseFunc> &
ScriptedRegistry() {
  static std::unordered_map<std::string, EnemyAttackState::ScriptedPhaseFunc>
      registry;
  return registry;
}

// 予備動作のずれ量を進行度から求める
float EvaluateOffsetCurve(OffsetCurve curve, float progress) {
  const float p = std::clamp(progress, 0.0f, 1.0f);
  switch (curve) {
  case OffsetCurve::EaseOutQuad:
    return 1.0f - (1.0f - p) * (1.0f - p);
  case OffsetCurve::Hump:
    return std::sin(p * std::numbers::pi_v<float>);
  case OffsetCurve::SineIn:
    return std::sin(p * std::numbers::pi_v<float> * 0.5f);
  case OffsetCurve::Linear:
    return p;
  default:
    return 1.0f - std::pow(1.0f - p, 3.0f);
  }
}

} // namespace

void EnemyAttackState::RegisterScriptedPhase(const std::string &id,
                                             ScriptedPhaseFunc func) {
  ScriptedRegistry()[id] = std::move(func);
}

// ============================================================
// 開始
// ============================================================
void EnemyAttackState::Enter(BattleEnemy &enemy) {
  enemy.SetCanAct(false);
  enemy.ResetStateTimer();

  phaseIndex_ = 0;
  phaseTimer_ = 0.0f;
  loopIteration_ = 0;
  finished_ = false;
  baseY_ = enemy.GetTranslate().y;

  name_ = action_ ? ("Attack:" + action_->id) : "Attack:none";

  // 攻撃開始時に一度だけ相手を向き、突進方向を決める
  FaceTarget(enemy);

  if (!action_ || action_->phases.empty()) {
    finished_ = true;
    return;
  }
  BeginPhase(enemy, 0);
}

// ============================================================
// フェーズの入り処理
// ============================================================
void EnemyAttackState::BeginPhase(BattleEnemy &enemy, int phaseIndex) {
  phaseIndex_ = phaseIndex;
  phaseTimer_ = 0.0f;

  const AttackPhase *phase = CurrentPhase();
  if (!phase)
    return;

  // 予備動作やジャンプはこの位置を基準に動く
  phaseStartPos_ = enemy.GetTranslate();
  phaseStartYaw_ = enemy.GetRotationY();

  // 無敵の切り替えはフェーズ単位。カウンターの「起動中だけ無敵」を表現する。
  enemy.IsInvincible() = phase->invincible;

  // 突進系は開始時点の向きを進行方向として固定する
  if (phase->type == AttackPhaseType::Dash) {
    FaceTarget(enemy);
  }

  // ジャンプは開始時の相手位置を着地点にする
  if (phase->type == AttackPhaseType::Leap) {
    leapTargetPos_ =
        enemy.HasPlayer() ? enemy.GetPlayerPosition() : phaseStartPos_;
  }

  // 見た目
  if (auto *anim = enemy.GetAnimation()) {
    const float scaleTime =
        (phase->scaleTime > 0.0f) ? phase->scaleTime : phase->duration;
    if (phase->scaleFrom.x != phase->scaleTo.x ||
        phase->scaleFrom.y != phase->scaleTo.y ||
        phase->scaleFrom.z != phase->scaleTo.z) {
      anim->StartRelativeScaleAnimation(phase->scaleFrom, phase->scaleTo,
                                        scaleTime);
    }
    if (phase->useColor) {
      anim->StartColorAnimation(anim->GetCurrentColor(), phase->color,
                                std::min(0.2f, phase->duration));
    }
    if (phase->shake) {
      anim->PlayShakeAnimation(phase->shakePower, phase->duration);
    }
  } else if (phase->useColor) {
    enemy.SetColor(phase->color);
  }

  if (!phase->vfxName.empty()) {
    EffectHandle::PlayOneShot(phase->vfxName, enemy.GetTranslate());
  }
}

const AttackPhase *EnemyAttackState::CurrentPhase() const {
  if (!action_)
    return nullptr;
  if (phaseIndex_ < 0 ||
      phaseIndex_ >= static_cast<int>(action_->phases.size()))
    return nullptr;
  return &action_->phases[phaseIndex_];
}

// ============================================================
// 更新
// ============================================================
void EnemyAttackState::Update(BattleEnemy &enemy, float dt) {
  if (finished_ || !action_) {
    enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
    return;
  }

  const AttackPhase *phase = CurrentPhase();
  if (!phase) {
    finished_ = true;
    enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
    return;
  }

  phaseTimer_ += dt;
  const float duration = std::max(0.0001f, phase->duration);
  const float progress = std::clamp(phaseTimer_ / duration, 0.0f, 1.0f);

  // 追尾指定のあるフェーズは毎フレーム相手を向く
  if (phase->faceTarget) {
    FaceTarget(enemy);
  }

  switch (phase->type) {
  case AttackPhaseType::Anticipation:
    UpdateAnticipation(enemy, *phase, progress);
    break;
  case AttackPhaseType::Dash:
    UpdateDash(enemy, *phase, dt);
    break;
  case AttackPhaseType::Spin:
    UpdateSpin(enemy, *phase, progress, dt);
    break;
  case AttackPhaseType::Leap:
    UpdateLeap(enemy, *phase, progress);
    break;

  case AttackPhaseType::Scripted: {
    auto it = ScriptedRegistry().find(phase->scriptedId);
    if (it != ScriptedRegistry().end()) {
      it->second(enemy, progress, dt);
    }
    break;
  }

  default: // Charge / Wait は位置を動かさない
    break;
  }

  if (phaseTimer_ < duration)
    return;

  // ------------------------------------------------------------
  // 次のフェーズへ。繰り返し区間の終端なら先頭へ戻る。
  // ------------------------------------------------------------
  const bool hasLoop =
      action_->loopBegin >= 0 && action_->loopEnd > action_->loopBegin;
  const int nextIndex = phaseIndex_ + 1;

  if (hasLoop && nextIndex == action_->loopEnd &&
      loopIteration_ + 1 < action_->loopCount) {
    ++loopIteration_;
    BeginPhase(enemy, action_->loopBegin);
    return;
  }

  if (nextIndex >= static_cast<int>(action_->phases.size())) {
    finished_ = true;
    enemy.ChangeState(SpacingSelector::SelectAfterAttack(enemy));
    return;
  }

  BeginPhase(enemy, nextIndex);
}

// ============================================================
// 終了
// ============================================================
void EnemyAttackState::Exit(BattleEnemy &enemy) {
  enemy.SetCanAct(true);
  enemy.IsInvincible() = false;

  // 上下に動かすフェーズがあるので、開始時の高さへ戻す
  Vector3 pos = enemy.GetTranslate();
  pos.y = baseY_;
  enemy.SetTranslate(pos);

  if (auto *anim = enemy.GetAnimation()) {
    anim->StopAll();
    anim->StartScaleAnimation(anim->GetCurrentScale(), anim->GetBaseScale(),
                              0.2f);
    anim->StartColorAnimation(anim->GetCurrentColor(), {1.0f, 1.0f, 1.0f, 1.0f},
                              0.2f);
  } else {
    enemy.SetColor({1.0f, 1.0f, 1.0f, 1.0f});
  }
}

// ============================================================
// 攻撃判定の番号
//
// 繰り返し区間では周回ごとに別の番号にして、
// コンボの各段が独立した1ヒットとして扱われるようにする。
// ============================================================
int EnemyAttackState::GetContactDamageWindow() const {
  const AttackPhase *phase = CurrentPhase();
  if (!phase || phase->damageWindow < 0)
    return -1;
  return phase->damageWindow + loopIteration_;
}

// ============================================================
// 予備動作：開始位置から指定方向へずれる
// ============================================================
void EnemyAttackState::UpdateAnticipation(BattleEnemy &enemy,
                                          const AttackPhase &phase,
                                          float progress) {
  // ひねり（Spin の溜め）は回転で表現する
  if (phase.rotations != 0.0f) {
    const float eased =
        progress < 0.5f ? 2.0f * progress * progress
                        : 1.0f - std::pow(-2.0f * progress + 2.0f, 2.0f) / 2.0f;
    enemy.SetRotationY(phaseStartYaw_ - phase.rotations * eased);
  }

  if (phase.distance == 0.0f)
    return;

  const float amount =
      phase.distance * EvaluateOffsetCurve(phase.offsetCurve, progress);

  // offsetAxis は敵から見たローカル方向。Z が前、Y が上。
  const float yaw = phaseStartYaw_;
  const Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
  const Vector3 right = {std::cos(yaw), 0.0f, -std::sin(yaw)};

  const Vector3 offset = right * (phase.offsetAxis.x * amount) +
                         Vector3{0.0f, phase.offsetAxis.y * amount, 0.0f} +
                         forward * (phase.offsetAxis.z * amount);

  Vector3 pos = phaseStartPos_ + offset;
  pos.y = std::max(0.0f, pos.y);
  enemy.SetTranslate(pos);
}

// ============================================================
// 突進：決めた方向へ直進する
// ============================================================
void EnemyAttackState::UpdateDash(BattleEnemy &enemy, const AttackPhase &phase,
                                  float dt) {
  // ホーミング指定があれば進行方向を相手へ寄せる
  if (phase.homing > 0.0f && enemy.HasPlayer()) {
    Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
    toPlayer.y = 0.0f;
    if (Length(toPlayer) > 0.01f) {
      toPlayer = Normalize(toPlayer);
      const float t = std::clamp(phase.homing * dt, 0.0f, 1.0f);
      attackDir_ = Normalize(attackDir_ + (toPlayer - attackDir_) * t);
      enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
    }
  }

  // 繰り返すたびに速度が上がる（コンボの加速）
  const float speed =
      phase.speedMultiplier +
      action_->loopSpeedGain * static_cast<float>(loopIteration_);
  enemy.AddTranslate(attackDir_ * enemy.GetEnemyData().moveSpeed * speed * dt);
}

// ============================================================
// 回転攻撃：回りながら相手へ寄る
// ============================================================
void EnemyAttackState::UpdateSpin(BattleEnemy &enemy, const AttackPhase &phase,
                                  float progress, float dt) {
  const float totalRotation =
      phase.rotations * 2.0f * std::numbers::pi_v<float>;
  enemy.SetRotationY(phaseStartYaw_ + progress * totalRotation);

  if (phase.speedMultiplier != 0.0f && enemy.HasPlayer()) {
    Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
    dir.y = 0.0f;
    if (Length(dir) > 0.01f) {
      dir = Normalize(dir);
      enemy.AddTranslate(dir * enemy.GetEnemyData().moveSpeed *
                         phase.speedMultiplier * dt);
    }
  }
}

// ============================================================
// ジャンプ：開始位置から着地点まで放物線で跳ぶ
// ============================================================
void EnemyAttackState::UpdateLeap(BattleEnemy &enemy, const AttackPhase &phase,
                                  float progress) {
  Vector3 pos;
  pos.x = phaseStartPos_.x + (leapTargetPos_.x - phaseStartPos_.x) * progress;
  pos.z = phaseStartPos_.z + (leapTargetPos_.z - phaseStartPos_.z) * progress;

  // 4t(1-t) で頂点が中間、両端が0の放物線になる
  const float heightOffset =
      phase.height * (4.0f * progress * (1.0f - progress));
  pos.y = std::max(0.0f, baseY_ + heightOffset);

  enemy.SetTranslate(pos);
}

// ============================================================
// 相手を向き、突進方向を更新する
// ============================================================
void EnemyAttackState::FaceTarget(BattleEnemy &enemy) {
  if (!enemy.HasPlayer())
    return;

  Vector3 dir = enemy.GetPlayerPosition() - enemy.GetTranslate();
  dir.y = 0.0f;
  if (Length(dir) < 0.01f)
    return;

  attackDir_ = Normalize(dir);
  enemy.SetRotationY(std::atan2(attackDir_.x, attackDir_.z));
}
