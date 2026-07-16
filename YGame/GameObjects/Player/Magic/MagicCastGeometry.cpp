#include "MagicCastGeometry.h"

#include "Collision/Core/CollisionManager.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "GameObjects/Player/Player.h"

#include <cmath>

namespace {
constexpr float kEyeHeight = 1.25f;    // 詠唱起点の高さ（胸〜頭付近）
constexpr float kForwardOffset = 1.2f; // 体の前に少し出す距離

constexpr float kGroundProbeUp = 3.0f;      // レイ始点を対象位置から持ち上げる量
constexpr float kGroundProbeDistance = 60.0f; // 下向きレイの最大距離
constexpr float kGroundLift = 0.05f;        // Zファイト回避に地面からわずかに浮かせる
} // namespace

namespace MagicCastGeometry {

Vector3 ResolveForward(Player &player) {
  const float yaw = player.GetCameraRotation().y;
  Vector3 forward{std::sin(yaw), 0.0f, std::cos(yaw)};
  forward = Vector3::Normalize(forward);
  if (std::abs(forward.x) < 0.0001f && std::abs(forward.z) < 0.0001f) {
    return Vector3{0.0f, 0.0f, 1.0f};
  }
  return forward;
}

Vector3 ResolveCastOrigin(Player &player) {
  return player.GetWorldPosition() + Vector3{0.0f, kEyeHeight, 0.0f} +
         ResolveForward(player) * kForwardOffset;
}

Vector3 ResolveGroundPoint(const Vector3 &position) {
  // 対象のやや上から下向きに地形へレイを撃つ（対象自身が地面に半分埋まっていても拾える）。
  Ray ray;
  ray.origin = position + Vector3{0.0f, kGroundProbeUp, 0.0f};
  ray.direction = Vector3{0.0f, -1.0f, 0.0f};

  static const std::vector<uint32_t> kGroundTypes = {
      static_cast<uint32_t>(CollisionTypeIdDef::kGroundSurface),
      static_cast<uint32_t>(CollisionTypeIdDef::kStaticWall),
      static_cast<uint32_t>(CollisionTypeIdDef::kNavObstacle),
  };

  RaycastHit hit;
  if (YoRigine::CollisionManager::GetInstance()->RaycastAllowTypes(
          ray, kGroundProbeDistance, &hit, kGroundTypes)) {
    return {position.x, hit.hitPoint.y + kGroundLift, position.z};
  }

  // 地形コライダーが無いエリアでは Y=0 の地平面へフォールバック
  // （現状の Ground はコライダーを持たない描画専用オブジェクトのため）。
  return {position.x, kGroundLift, position.z};
}

} // namespace MagicCastGeometry
