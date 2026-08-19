#include "AttackPlayer.h"

#include "../../BaseEnemy.h"
#include "../Motion/MotionPathLibrary.h"
#include "../Projectile/ProjectileManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// ローカル（前方＝+Z）のオフセットを、指定ヨーでワールドへ回す
Vector3 RotateOffsetByYaw(const Vector3 &local, float yaw) {
  const Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
  const Vector3 right = {std::cos(yaw), 0.0f, -std::sin(yaw)};
  return right * local.x + Vector3{0.0f, local.y, 0.0f} + forward * local.z;
}

} // namespace

// ============================================================
// 再生開始
// ============================================================
void AttackPlayer::Play(BaseEnemy &enemy, const EnemyAttack *attack) {
  attack_ = attack;
  playing_ = (attack != nullptr);
  finished_ = false;
  time_ = 0.0f;
  homingOffset_ = {};

  // 相対量の基準。ここへ戻せばいつでも元の姿勢に復帰できる。
  basePosition_ = enemy.GetTranslate();
  baseRotation_ = enemy.GetRotate();
  baseScale_ = enemy.GetScale();

  instantFired_.assign(attack ? attack->modifiers.size() : 0, false);

  if (playing_) {
    ApplyPose(enemy);
  }
}

// ============================================================
// 停止（基準姿勢へ復帰）
// ============================================================
void AttackPlayer::Stop(BaseEnemy &enemy) {
  if (!playing_)
    return;

  enemy.SetTranslate(basePosition_);
  enemy.SetRotate(baseRotation_);
  enemy.SetScale(baseScale_);

  playing_ = false;
  finished_ = false;
  time_ = 0.0f;
  homingOffset_ = {};
  attack_ = nullptr;
}

// ============================================================
// 停止（回転とスケールだけ復帰）
// ============================================================
void AttackPlayer::StopKeepPosition(BaseEnemy &enemy) {
  if (!playing_) return;

  enemy.SetRotate(baseRotation_);
  enemy.SetScale(baseScale_);

  playing_ = false;
  finished_ = false;
  time_ = 0.0f;
  homingOffset_ = {};
  attack_ = nullptr;
}

float AttackPlayer::GetNormalizedTime() const {
  if (!attack_ || attack_->duration <= 0.0f)
    return 0.0f;
  return std::clamp(time_ / attack_->duration, 0.0f, 1.0f);
}

// ============================================================
// 更新
// ============================================================
void AttackPlayer::Update(BaseEnemy &enemy, float deltaTime) {
  if (!playing_ || !attack_)
    return;

  const float previousTime = time_;
  time_ += deltaTime;

  // 追尾は毎フレームの積み上げなので、姿勢計算より先に更新する
  ApplyRangeModifiers(enemy, deltaTime);
  FireInstantModifiers(enemy, previousTime);

  ApplyPose(enemy);

  if (time_ >= attack_->duration) {
    time_ = attack_->duration;
    finished_ = true;
  }
}

// ============================================================
// 任意時刻へ移動（エディタのスクラブ用）
//
// 弾の発射は時間を戻したときに再発火させたいので、
// 発火済みフラグを作り直してから進める。
// ============================================================
void AttackPlayer::Seek(BaseEnemy &enemy, float time) {
  if (!playing_ || !attack_)
    return;

  time_ = std::clamp(time, 0.0f, attack_->duration);
  homingOffset_ = {};
  instantFired_.assign(attack_->modifiers.size(), false);
  finished_ = (time_ >= attack_->duration);

  ApplyPose(enemy);
}

// ============================================================
// 位置オフセット
//
// カーブか経路のどちらかから取る。どちらも「開始時の姿勢基準の
// ローカル座標」で返すので、この後まとめてワールドへ回す。
// ============================================================
Vector3 AttackPlayer::EvaluatePositionOffset(const BaseEnemy &enemy,
                                             float normalizedTime) const {
  if (attack_->positionSource == AttackPositionSource::Path &&
      !attack_->pathName.empty()) {
    const IAttackMotion *path =
        MotionPathLibrary::GetInstance().Find(attack_->pathName);
    if (path) {
      // 経路はワールド座標を返すので、基準位置との差分に直す
      MotionContext ctx;
      ctx.startPosition = basePosition_;
      ctx.startYaw = baseRotation_.y;
      if (enemy.HasPlayer()) {
        ctx.targetPosition = enemy.GetPlayerPosition();
        ctx.hasTarget = true;
      }

      const Vector3 world = path->Evaluate(normalizedTime, ctx);
      const Vector3 delta = world - basePosition_;

      // ワールド差分をローカルへ戻す（この後もう一度回すため）
      const float yaw = baseRotation_.y;
      const Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
      const Vector3 right = {std::cos(yaw), 0.0f, -std::sin(yaw)};
      return {Dot(delta, right), delta.y, Dot(delta, forward)};
    }
  }
  return attack_->tracks.EvaluatePositionOffset(normalizedTime);
}

// ============================================================
// 姿勢の適用
// ============================================================
void AttackPlayer::ApplyPose(BaseEnemy &enemy) {
  const float t = GetNormalizedTime();

  // 位置：ローカルオフセットを開始時の向きで回してから基準へ足す
  const Vector3 localOffset = EvaluatePositionOffset(enemy, t);
  Vector3 position =
      basePosition_ + RotateOffsetByYaw(localOffset, baseRotation_.y);
  position += homingOffset_;
  position.y = std::max(0.0f, position.y); // 地面より下へ潜らせない
  enemy.SetTranslate(position);

  // 回転：基準角度からの差分
  const Vector3 rotationOffset = attack_->tracks.EvaluateRotationOffset(t);
  enemy.SetRotate(baseRotation_ + rotationOffset);

  // スケール：基準スケールへの倍率
  const Vector3 scaleMultiplier = attack_->tracks.EvaluateScaleMultiplier(t);
  enemy.SetScale({baseScale_.x * scaleMultiplier.x,
                  baseScale_.y * scaleMultiplier.y,
                  baseScale_.z * scaleMultiplier.z});
}

// ============================================================
// 区間モディファイア
// ============================================================
void AttackPlayer::ApplyRangeModifiers(BaseEnemy &enemy, float deltaTime) {
  if (!enemy.HasPlayer())
    return;

  for (const auto &modifier : attack_->modifiers) {
    if (modifier.IsInstant() || !modifier.IsActiveAt(time_))
      continue;

    switch (modifier.type) {
    case AttackModifierType::FaceTarget: {
      // 基準の向き自体を回す。以降のオフセットもこの向きに追従する。
      Vector3 toTarget = enemy.GetPlayerPosition() - enemy.GetTranslate();
      toTarget.y = 0.0f;
      if (Length(toTarget) > 0.01f) {
        const float targetYaw = std::atan2(toTarget.x, toTarget.z);
        float diff = targetYaw - baseRotation_.y;
        while (diff > std::numbers::pi_v<float>)
          diff -= 2.0f * std::numbers::pi_v<float>;
        while (diff < -std::numbers::pi_v<float>)
          diff += 2.0f * std::numbers::pi_v<float>;

        const float maxStep = modifier.strength * deltaTime;
        baseRotation_.y += std::clamp(diff, -maxStep, maxStep);
      }
      break;
    }

    case AttackModifierType::HomingOffset: {
      // カーブで作った軌道を相手方向へ曲げる。
      // 基準位置ではなく現在位置からの方向で寄せる。
      Vector3 toTarget = enemy.GetPlayerPosition() - enemy.GetTranslate();
      toTarget.y = 0.0f;
      if (Length(toTarget) > 0.01f) {
        homingOffset_ += Normalize(toTarget) * modifier.strength * deltaTime;
      }
      break;
    }

    default:
      break;
    }
  }
}

// ============================================================
// 瞬間モディファイア（弾の発射）
//
// フレーム間で時刻をまたいだものだけを1回発火する。
// ============================================================
void AttackPlayer::FireInstantModifiers(BaseEnemy &enemy, float previousTime) {
  auto *projectiles = ProjectileManager::GetCurrent();

  for (size_t i = 0; i < attack_->modifiers.size(); ++i) {
    const auto &modifier = attack_->modifiers[i];
    if (!modifier.IsInstant() || instantFired_[i])
      continue;
    if (modifier.startTime > time_ || modifier.startTime < previousTime)
      continue;

    instantFired_[i] = true;
    if (!projectiles)
      continue;

    // 発射位置は敵のローカルオフセット
    const float yaw = enemy.GetRotationY();
    const Vector3 spawnPos =
        enemy.GetTranslate() + RotateOffsetByYaw(modifier.offset, yaw);

    // 基準方向。相手を狙うか、正面へ撃つか。
    Vector3 baseDir = {std::sin(yaw), 0.0f, std::cos(yaw)};
    if (modifier.aimAtTarget && enemy.HasPlayer()) {
      Vector3 toTarget = enemy.GetPlayerPosition() - spawnPos;
      toTarget.y = 0.0f;
      if (Length(toTarget) > 0.01f)
        baseDir = Normalize(toTarget);
    }

    // count 発を扇状に散らす
    const int count = std::max(1, modifier.count);
    const float baseYaw = std::atan2(baseDir.x, baseDir.z);
    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
    const float spreadRad = modifier.spreadDeg * kDegToRad;
    const float step =
        (count > 1) ? spreadRad / static_cast<float>(count - 1) : 0.0f;
    const float startYaw = baseYaw - spreadRad * 0.5f;

    for (int shot = 0; shot < count; ++shot) {
      const float shotYaw =
          (count > 1) ? startYaw + step * static_cast<float>(shot) : baseYaw;
      const Vector3 dir = {std::sin(shotYaw), 0.0f, std::cos(shotYaw)};
      projectiles->Spawn(modifier.projectileId, spawnPos, dir);
    }
  }
}

// ============================================================
// 攻撃判定と無敵
// ============================================================
int AttackPlayer::GetActiveDamageWindow() const {
  if (!playing_ || !attack_)
    return -1;

  for (const auto &modifier : attack_->modifiers) {
    if (modifier.type != AttackModifierType::Hitbox)
      continue;
    if (modifier.IsActiveAt(time_))
      return modifier.damageWindow;
  }
  return -1;
}

bool AttackPlayer::IsInvincibleNow() const {
  if (!playing_ || !attack_)
    return false;

  for (const auto &modifier : attack_->modifiers) {
    if (modifier.type != AttackModifierType::Invincible)
      continue;
    if (modifier.IsActiveAt(time_))
      return true;
  }
  return false;
}
