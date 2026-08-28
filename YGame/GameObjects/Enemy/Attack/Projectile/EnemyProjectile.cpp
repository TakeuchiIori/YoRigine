#include "EnemyProjectile.h"

#include "Player/Player.h"

#include <algorithm>
#include <cmath>

EnemyProjectile::EnemyProjectile(const ProjectileDefinition &definition,
                                 const Vector3 &position,
                                 const Vector3 &direction, Player *target)
    : definition_(definition), target_(target) {

  wt_.Initialize();
  wt_.translate_ = position;
  wt_.scale_ = definition_.scale;

  startPosition_ = position;
  startYaw_ = std::atan2(direction.x, direction.z);

  velocity_ = direction * definition_.speed;

  if (!definition_.modelPath.empty()) {
    obj_ = std::make_unique<YoRigine::Object3d>();
    obj_->Initialize();
    obj_->SetModel(definition_.modelPath);
    obj_->SetMaterialColor(definition_.color);
  }

  if (!definition_.trailVfxName.empty()) {
    trailVfx_ =
        EffectHandle::Play(definition_.trailVfxName, position, /*loop*/ true,
                           /*emitCount*/ -1);
  }

  ApplyFacing(direction);
  wt_.UpdateMatrix();
}

EnemyProjectile::~EnemyProjectile() {
  // 弾が消えたのに軌跡だけ残らないように必ず止める
  if (trailVfx_.IsValid()) {
    trailVfx_.Stop();
  }
}

// ============================================================
// 更新
// ============================================================
void EnemyProjectile::Update(float deltaTime) {
  if (!alive_)
    return;

  age_ += deltaTime;
  if (age_ >= definition_.lifeTime) {
    alive_ = false;
    return;
  }

  if (definition_.mode == ProjectileMotionMode::Path && definition_.path) {
    StepPath();
  } else {
    StepStraight(deltaTime);
  }

  if (trailVfx_.IsValid()) {
    trailVfx_.SetPosition(wt_.translate_);
  }
  wt_.UpdateMatrix();
}

// ============================================================
// 直進
//
// 重力とホーミングを速度に加えてから進む。
// homing を上げるほど相手へ曲がっていく。
// ============================================================
void EnemyProjectile::StepStraight(float deltaTime) {
  if (definition_.homing > 0.0f && target_) {
    Vector3 toTarget = target_->GetWorldPosition() - wt_.translate_;
    toTarget.y += 1.0f; // 足元ではなく胴を狙う
    if (Length(toTarget) > 0.01f) {
      toTarget = Normalize(toTarget);
      const float speed = Length(velocity_);
      const float t = std::clamp(definition_.homing * deltaTime, 0.0f, 1.0f);
      Vector3 dir =
          Normalize(velocity_) + (toTarget - Normalize(velocity_)) * t;
      velocity_ = Normalize(dir) * speed;
    }
  }

  if (definition_.gravity != 0.0f) {
    velocity_.y -= definition_.gravity * deltaTime;
  }

  wt_.translate_ += velocity_ * deltaTime;
  ApplyFacing(velocity_);
}

// ============================================================
// 経路に沿って進む
//
// 寿命に対する経過割合をそのまま経路の進行度として使う。
// 経路が TargetRelative なら相手の移動に合わせて曲がる。
// ============================================================
void EnemyProjectile::StepPath() {
  MotionContext ctx;
  ctx.startPosition = startPosition_;
  ctx.startYaw = startYaw_;
  if (target_) {
    ctx.targetPosition = target_->GetWorldPosition();
    ctx.hasTarget = true;
  }

  const float t =
      std::clamp(age_ / std::max(0.0001f, definition_.lifeTime), 0.0f, 1.0f);

  const Vector3 next = definition_.path->Evaluate(t, ctx);
  const Vector3 delta = next - wt_.translate_;
  wt_.translate_ = next;

  ApplyFacing(delta);
}

// ============================================================
// 進行方向へ機首を向ける
// ============================================================
void EnemyProjectile::ApplyFacing(const Vector3 &direction) {
  if (!definition_.faceVelocity)
    return;
  if (Length(direction) < 0.0001f)
    return;

  const Vector3 dir = Normalize(direction);
  wt_.rotate_.y = std::atan2(dir.x, dir.z);
  wt_.rotate_.x = -std::asin(std::clamp(dir.y, -1.0f, 1.0f));
}

// ============================================================
// 着弾
// ============================================================
void EnemyProjectile::OnHit() {
  if (!definition_.hitVfxName.empty()) {
    EffectHandle::PlayOneShot(definition_.hitVfxName, wt_.translate_);
  }
  if (definition_.destroyOnHit) {
    alive_ = false;
  }
}

// ============================================================
// 描画
// ============================================================
void EnemyProjectile::Draw(YoRigine::Camera *camera) {
  if (!alive_ || !obj_)
    return;
  obj_->Draw(camera, wt_);
}
