#include "MagicCastGeometry.h"

#include "GameObjects/Player/Player.h"

#include <cmath>

namespace {
constexpr float kEyeHeight = 1.25f;    // 詠唱起点の高さ（胸〜頭付近）
constexpr float kForwardOffset = 1.2f; // 体の前に少し出す距離
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

} // namespace MagicCastGeometry
