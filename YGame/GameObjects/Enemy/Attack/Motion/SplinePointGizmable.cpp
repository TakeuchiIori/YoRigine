#include "SplinePointGizmable.h"

#ifdef USE_IMGUI

SplinePointGizmable::SplinePointGizmable(
    SplineMotion *spline, int pointIndex, const MotionContext *context,
    std::function<void()> onManipulationEnd)
    : spline_(spline), pointIndex_(pointIndex), context_(context),
      onManipulationEnd_(std::move(onManipulationEnd)) {}

std::string SplinePointGizmable::GetGizmoLabel() const {
  return "SplinePoint" + std::to_string(pointIndex_);
}

// ============================================================
// 制御点のワールド位置
// ============================================================
Vector3 SplinePointGizmable::GetGizmoPosition() const {
  if (!spline_ || !context_)
    return {};
  if (pointIndex_ < 0 ||
      pointIndex_ >= static_cast<int>(spline_->points.size()))
    return {};

  return context_->LocalToWorld(spline_->points[pointIndex_], spline_->space);
}

// ============================================================
// ギズモで動かされた位置を制御点へ書き戻す
// ============================================================
void SplinePointGizmable::SetGizmoPosition(const Vector3 &pos) {
  if (!spline_ || !context_)
    return;
  if (pointIndex_ < 0 ||
      pointIndex_ >= static_cast<int>(spline_->points.size()))
    return;

  spline_->points[pointIndex_] = context_->WorldToLocal(pos, spline_->space);
}

void SplinePointGizmable::OnGizmoManipulationEnd() {
  if (onManipulationEnd_)
    onManipulationEnd_();
}

#endif // USE_IMGUI
