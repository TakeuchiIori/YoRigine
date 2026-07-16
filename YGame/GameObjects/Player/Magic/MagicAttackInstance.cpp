#include "MagicAttackInstance.h"

#include "Collision/AABB/AABBCollider.h"
#include "Collision/Core/ColliderFactory.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Collision/Sphere/SphereCollider.h"
#include "GameObjects/Enemy/BattleEnemy/BattleEnemy.h"
#include "GameObjects/Player/Camera/PlayerCamera.h"
#include "GameObjects/Player/Player.h"
#include "MagicCastGeometry.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"
#include "Systems/GameTime/GameTime.h"
#include "UI/Damage/DamageNumberManager.h"

#include <algorithm>
#include <cmath>

MagicAttackInstance::~MagicAttackInstance() {
  // 飛翔中に破棄された場合（シーン遷移など）でも追従VFXを取り残さない。
  if (travelVfx_.IsValid())
    travelVfx_.Stop();
}

void MagicAttackInstance::Initialize(const MagicActionData &action,
                                     Player *owner) {
  action_ = action;
  owner_ = owner;
  elapsedTime_ = 0.0f;
  feedbackFired_ = false;
  died_ = false;
  lastHitTimes_.clear();

  wt_.Initialize();
  if (owner_) {
    origin_ = MagicCastGeometry::ResolveCastOrigin(*owner_);
    target_ = ResolveTarget(*owner_, origin_);
    attackPosition_ = origin_;
    wt_.translate_ = attackPosition_ + action_.hitOffset;
  }
  wt_.UpdateMatrix();

  Camera *camera = owner_ && owner_->GetPlayerCamera()
                       ? owner_->GetPlayerCamera()->GetLastCamera()
                       : nullptr;

  if (action_.hitShape == MagicHitShape::AABB) {
    auto aabb = ColliderFactory::Create<AABBCollider>(
        this, &wt_, camera,
        static_cast<uint32_t>(CollisionTypeIdDef::kPlayerMagic));
    if (aabb) {
      const Vector3 half = {
          std::max(0.01f, std::abs(action_.hitHalfExtents.x)),
          std::max(0.01f, std::abs(action_.hitHalfExtents.y)),
          std::max(0.01f, std::abs(action_.hitHalfExtents.z)),
      };
      aabb->aabbOffset_.min = {-half.x, -half.y, -half.z};
      aabb->aabbOffset_.max = {half.x, half.y, half.z};
    }
    collider_ = aabb;
  } else {
    auto sphere = ColliderFactory::Create<SphereCollider>(
        this, &wt_, camera,
        static_cast<uint32_t>(CollisionTypeIdDef::kPlayerMagic));
    if (sphere) {
      sphere->SetRadius(
          std::max(0.01f, action_.hitRadius * action_.scaleCurve.Evaluate(0.0f)));
    }
    collider_ = sphere;
  }

  if (collider_) {
    collider_->SetIsStatic(false);
    collider_->SetMass(0.0f);
    collider_->SetEnablePenetration(false);
    collider_->SetCheckOutsideCamera(false);
    collider_->SetCollisionMask(CollisionLayerBit(CollisionLayer::Enemy));
  }

  // 飛道弾の見た目は判定本体に追従させ、位置・寿命を判定と同期させる。
  if (!action_.travelVfx.empty()) {
    travelVfx_ = EffectHandle::Play(action_.travelVfx, attackPosition_,
                                    /*loop*/ true, /*emitCount*/ -1);
  }

  alive_ = true;
}

void MagicAttackInstance::Update(float deltaTime) {
  if (!alive_)
    return;

  elapsedTime_ += deltaTime;
  const float duration = std::max(0.01f, action_.duration);
  const float t = std::clamp(elapsedTime_ / duration, 0.0f, 1.0f);

  switch (action_.trajectoryType) {
  case MagicTrajectoryType::StrikeTarget:
    attackPosition_ = target_;
    break;
  case MagicTrajectoryType::Forward:
  case MagicTrajectoryType::LockOnOrForward:
  default:
    attackPosition_ = origin_ + (target_ - origin_) * t;
    break;
  }

	wt_.translate_ = attackPosition_ + action_.hitOffset;
  wt_.UpdateMatrix();
  if (collider_) {
    const bool active = elapsedTime_ >= action_.hitDelay;
    collider_->SetCollisionEnabled(active);
    if (auto *sphere = dynamic_cast<SphereCollider *>(collider_.get())) {
      sphere->SetRadius(
          std::max(0.01f, action_.hitRadius * action_.scaleCurve.Evaluate(t)));
    } else if (auto *aabb = dynamic_cast<AABBCollider *>(collider_.get())) {
      const float scale = std::max(0.01f, action_.scaleCurve.Evaluate(t));
      const Vector3 half = {
          std::max(0.01f, std::abs(action_.hitHalfExtents.x) * scale),
          std::max(0.01f, std::abs(action_.hitHalfExtents.y) * scale),
          std::max(0.01f, std::abs(action_.hitHalfExtents.z) * scale),
      };
      aabb->aabbOffset_.min = {-half.x, -half.y, -half.z};
      aabb->aabbOffset_.max = {half.x, half.y, half.z};
    }
    collider_->Update();
  }

  // 追従VFXを判定本体の現在位置へ。
  if (travelVfx_.IsValid())
    travelVfx_.SetPosition(attackPosition_);

  if (elapsedTime_ >= duration) {
    Die();
  }
}

void MagicAttackInstance::DrawCollision() {
  if (!collider_ || !alive_)
    return;
  if (auto *sphere = dynamic_cast<SphereCollider *>(collider_.get()))
    sphere->Draw();
  else if (auto *aabb = dynamic_cast<AABBCollider *>(collider_.get()))
    aabb->Draw();
}

void MagicAttackInstance::OnEnterCollision([[maybe_unused]] BaseCollider *self,
                                           BaseCollider *other) {
  ApplyHit(other);
}

void MagicAttackInstance::OnCollision([[maybe_unused]] BaseCollider *self,
                                      BaseCollider *other) {
  // 滞在中も判定する。単発かどうかは ApplyHit 側が hitInterval で判断するため、
  // ここは毎フレーム渡すだけでよい（設置型が敵に居座っても周期ヒットになる）。
  ApplyHit(other);
}
void MagicAttackInstance::OnExitCollision(
    [[maybe_unused]] BaseCollider *self, [[maybe_unused]] BaseCollider *other) {
}
void MagicAttackInstance::OnDirectionCollision(
    [[maybe_unused]] BaseCollider *self, [[maybe_unused]] BaseCollider *other,
    [[maybe_unused]] HitDirection dir) {}
void MagicAttackInstance::OnEnterDirectionCollision(
    [[maybe_unused]] BaseCollider *self, [[maybe_unused]] BaseCollider *other,
    [[maybe_unused]] HitDirection dir) {}

Vector3 MagicAttackInstance::ResolveTarget(Player &owner,
                                           const Vector3 &origin) const {
  if (action_.trajectoryType != MagicTrajectoryType::Forward) {
    if (PlayerCamera *camera = owner.GetPlayerCamera()) {
      if (BaseCollider *target = camera->GetLockedTarget()) {
        return target->GetCenterPosition();
      }
    }
  }

  return origin + MagicCastGeometry::ResolveForward(owner) *
                      std::max(0.0f, action_.range);
}

void MagicAttackInstance::ApplyHit(BaseCollider *other) {
  if (!alive_ || died_)
    return;
  if (!other)
    return;
  if (other->GetTypeID() !=
      static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy))
    return;

  // 再ヒット判定。hitInterval<=0 なら 1 体 1 回、>0
  // ならその間隔でのみ再ヒット。
  const auto it = lastHitTimes_.find(other);
  if (it != lastHitTimes_.end()) {
    if (action_.hitInterval <= 0.0f)
      return;
    if (elapsedTime_ - it->second < action_.hitInterval)
      return;
  }

  BattleEnemy *enemy = other->GetOwnerAs<BattleEnemy>();
  if (!enemy || !enemy->IsAlive())
    return;

  lastHitTimes_[other] = elapsedTime_;

  // ダメージは判定半径と切り離し、魔法データの damage を正とする。
  // event.power は VFX
  // 強度など見た目側の指標として残し、当たりダメージには混ぜない。
  const int damage = static_cast<int>(std::max(0.0f, action_.damage));
  enemy->TakeDamage(damage);
  DamageNumberManager::GetInstance()->SpawnDamage(damage, enemy->GetTranslate(),
                                                  false);

  // 手応え(ヒットストップ/シェイク)はインスタンスの最初の命中でだけ発火する。
  // 貫通で複数体に当たった時やエリアの再ヒットで連発させない。
  if (!feedbackFired_) {
    feedbackFired_ = true;
    if (action_.hitStopDuration > 0.0f) {
      YoRigine::GameTime::SetHitStop(action_.hitStopDuration);
    }
    if (action_.shakeIntensity > 0.0f && action_.shakeDuration > 0.0f) {
      if (owner_ && owner_->GetFollowCamera()) {
        owner_->GetFollowCamera()->StartShake(action_.shakeIntensity,
                                              action_.shakeDuration);
      }
    }
  }

  // 飛道弾は最初の命中で消滅させる（貫通しない）。設置型やビームは false
  // のまま。
  if (action_.destroyOnHit) {
    Die();
  }
}

void MagicAttackInstance::Die() {
  if (died_)
    return;
  died_ = true;
  alive_ = false;

  if (collider_)
    collider_->SetCollisionEnabled(false);
  if (travelVfx_.IsValid())
    travelVfx_.Stop();
  if (!action_.impactVfx.empty()) {
    // Compositeを先に解決する統一入口から再生し、新規CPU素材も着弾へ含める。
    EffectHandle::PlayOneShot(action_.impactVfx, attackPosition_, -1);
  }
}
