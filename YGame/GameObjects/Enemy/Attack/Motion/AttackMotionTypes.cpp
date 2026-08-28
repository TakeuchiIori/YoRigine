#include "AttackMotionTypes.h"

#include <cmath>

const char *MotionSpaceToString(MotionSpace space) {
  switch (space) {
  case MotionSpace::TargetRelative:
    return "targetRelative";
  case MotionSpace::World:
    return "world";
  default:
    return "selfLocal";
  }
}

MotionSpace MotionSpaceFromString(const std::string &name) {
  if (name == "targetRelative")
    return MotionSpace::TargetRelative;
  if (name == "world")
    return MotionSpace::World;
  return MotionSpace::SelfLocal;
}

// ============================================================
// ローカル座標をワールドへ
//
// ローカルは「前方＝+Z / 右＝+X / 上＝+Y」。
// 攻撃開始時の向きを基準にするので、途中で敵が回転しても
// 経路の形は崩れない。
// ============================================================
Vector3 MotionContext::LocalToWorld(const Vector3 &local,
                                    MotionSpace space) const {
  if (space == MotionSpace::World) {
    return local * scale;
  }

  const Vector3 scaled = local * scale;

  // 開始時のヨーから前方・右方向を作る
  const Vector3 forward = {std::sin(startYaw), 0.0f, std::cos(startYaw)};
  const Vector3 right = {std::cos(startYaw), 0.0f, -std::sin(startYaw)};

  const Vector3 offset =
      right * scaled.x + Vector3{0.0f, scaled.y, 0.0f} + forward * scaled.z;

  // TargetRelative は相手の現在位置が原点。相手が動けば経路も動く。
  if (space == MotionSpace::TargetRelative && hasTarget) {
    return targetPosition + offset;
  }
  return startPosition + offset;
}

// ============================================================
// ワールド座標をローカルへ（LocalToWorld の逆）
//
// 前方・右方向は水平面で直交しているので、内積で成分を取り出せる。
// ============================================================
Vector3 MotionContext::WorldToLocal(const Vector3 &world,
                                    MotionSpace space) const {
  const float safeScale = (std::abs(scale) > 0.0001f) ? scale : 1.0f;

  if (space == MotionSpace::World) {
    return world / safeScale;
  }

  const Vector3 origin = (space == MotionSpace::TargetRelative && hasTarget)
                             ? targetPosition
                             : startPosition;
  const Vector3 offset = world - origin;

  const Vector3 forward = {std::sin(startYaw), 0.0f, std::cos(startYaw)};
  const Vector3 right = {std::cos(startYaw), 0.0f, -std::sin(startYaw)};

  return {Dot(offset, right) / safeScale, offset.y / safeScale,
          Dot(offset, forward) / safeScale};
}
