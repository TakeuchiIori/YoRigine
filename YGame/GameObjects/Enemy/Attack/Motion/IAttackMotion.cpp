#include "IAttackMotion.h"

#include <algorithm>

// ============================================================
// 進行方向の既定実装
//
// 経路の式が分からなくても、少し先の位置との差分を取れば
// 向きは求まる。終端では手前との差分を使う。
// ============================================================
Vector3 IAttackMotion::EvaluateDirection(float t,
                                         const MotionContext &ctx) const {
  constexpr float kStep = 0.01f;

  const float ahead = std::min(1.0f, t + kStep);
  const float behind = std::max(0.0f, t - kStep);

  Vector3 delta = Evaluate(ahead, ctx) - Evaluate(behind, ctx);
  if (Length(delta) < 0.0001f) {
    return {0.0f, 0.0f, 1.0f};
  }
  return Normalize(delta);
}
