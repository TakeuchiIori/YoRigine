#pragma once

#ifdef USE_IMGUI

#include "Debugger/Gizmo/IGizmable.h"
#include "SplineMotion.h"

#include <functional>

// ============================================================
// スプラインの制御点1つをギズモで掴めるようにする薄いラッパ
//
// 制御点はローカル座標（開始位置と向きが基準）で持っているが、
// ギズモが扱うのはワールド座標なので、ここで往復変換する。
// これがないと「前方へ8m」の意味が失われ、敵の向きが変わると
// 経路の形も崩れてしまう。
//
// 実体は編集中だけ作る一時オブジェクトなので、状態は持たない。
// ============================================================
class SplinePointGizmable : public IGizmable {
public:
  SplinePointGizmable(SplineMotion *spline, int pointIndex,
                      const MotionContext *context,
                      std::function<void()> onManipulationEnd = nullptr);

  std::string GetGizmoLabel() const override;

  Vector3 GetGizmoPosition() const override;
  void SetGizmoPosition(const Vector3 &pos) override;

  // 制御点に回転・スケールの概念はないので何もしない
  Vector3 GetGizmoRotation() const override { return {}; }
  void SetGizmoRotation(const Vector3 &) override {}
  Vector3 GetGizmoScale() const override { return {1.0f, 1.0f, 1.0f}; }
  void SetGizmoScale(const Vector3 &) override {}

  void OnGizmoManipulationEnd() override;

  float GetGizmoPickRadius() const override { return 0.4f; }

private:
  SplineMotion *spline_ = nullptr;
  int pointIndex_ = -1;
  const MotionContext *context_ = nullptr;
  std::function<void()> onManipulationEnd_;
};

#endif // USE_IMGUI
