#include "OrbitMotion.h"

#include <algorithm>
#include <cmath>
#include <numbers>

// ============================================================
// 評価
//
// 半径と高さを開始値から終了値へ補間しながら角度を進める。
// 半径を縮めていけば「回り込みながら間合いを詰める」動きになる。
// ============================================================
Vector3 OrbitMotion::Evaluate(float t, const MotionContext &ctx) const {
  const float clamped = std::clamp(t, 0.0f, 1.0f);

  constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
  const float angle = (startAngleDeg + sweepDeg * clamped) * kDegToRad;

  const float radius = startRadius + (endRadius - startRadius) * clamped;
  const float height = startHeight + (endHeight - startHeight) * clamped;

  // 中心から見た円周上の位置（ローカル：前方＝+Z）
  const Vector3 local = {std::sin(angle) * radius, height,
                         std::cos(angle) * radius};

  return ctx.LocalToWorld(local, space);
}

std::unique_ptr<IAttackMotion> OrbitMotion::Clone() const {
  auto copy = std::make_unique<OrbitMotion>();
  *copy = *this;
  return copy;
}
