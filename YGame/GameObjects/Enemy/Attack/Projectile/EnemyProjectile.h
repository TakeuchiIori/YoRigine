#pragma once

#include "ProjectileDefinition.h"

#include "Object3D/Object3d.h"
#include "Particle/EffectHandle.h"
#include "WorldTransform/WorldTransform.h"

#include <memory>

namespace YoRigine {
class Camera;
}
class Player;

// ============================================================
// 敵が撃つ弾1発
//
// 飛び方は ProjectileDefinition が決めるので、このクラスは
// 「定義に従って動かして、当たったら消える」だけを持つ。
// 直進も曲がる弾も追尾弾も、同じこのクラスで動く。
// ============================================================
class EnemyProjectile {
public:
  EnemyProjectile(const ProjectileDefinition &definition,
                  const Vector3 &position, const Vector3 &direction,
                  Player *target);
  ~EnemyProjectile();

  void Update(float deltaTime);
  void Draw(YoRigine::Camera *camera);

  bool IsAlive() const { return alive_; }
  void Kill() { alive_ = false; }

  const Vector3 &GetPosition() const { return wt_.translate_; }
  float GetRadius() const { return definition_.radius; }
  int GetDamage() const { return definition_.damage; }
  bool DestroyOnHit() const { return definition_.destroyOnHit; }

  // 着弾処理（当たった側から呼ぶ）
  void OnHit();

private:
  // 直進（重力とホーミングを含む）
  void StepStraight(float deltaTime);

  // 経路に沿って進む
  void StepPath();

  // 進行方向へ機首を向ける
  void ApplyFacing(const Vector3 &direction);

private:
  ProjectileDefinition definition_;

  YoRigine::WorldTransform wt_;
  std::unique_ptr<YoRigine::Object3d> obj_;

  Player *target_ = nullptr;

  Vector3 velocity_{};
  Vector3 startPosition_{};
  float startYaw_ = 0.0f;

  float age_ = 0.0f;
  bool alive_ = true;

  // 飛行中に追従させるVFX
  EffectHandle trailVfx_;
};
