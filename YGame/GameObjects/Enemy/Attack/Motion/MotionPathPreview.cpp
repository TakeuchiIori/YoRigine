#include "MotionPathPreview.h"

#ifdef USE_IMGUI

#include "Graphics/Drawer/LineManager/Line.h"

#include <algorithm>

namespace {
// 経路を折れ線で近似するときの分割数。多いほど滑らかに見える。
constexpr int kPathSamples = 48;
constexpr int kSphereResolution = 8;
} // namespace

void MotionPathPreview::Initialize(YoRigine::Camera *camera) {
  camera_ = camera;

  auto makeLine = [camera]() {
    auto line = std::make_unique<YoRigine::Line>();
    line->Initialize();
    line->SetCamera(camera);
    return line;
  };

  pathLine_ = makeLine();
  pointLine_ = makeLine();
  selectedLine_ = makeLine();
  markerLine_ = makeLine();
}

// ============================================================
// 経路の描画
// ============================================================
void MotionPathPreview::Draw(const IAttackMotion &motion,
                             const MotionContext &context,
                             int selectedPointIndex, float markerT) {
  if (!pathLine_)
    return;

  // 前フレームの頂点が残らないよう毎回リセットする
  pathLine_->Reset();
  pointLine_->Reset();
  selectedLine_->Reset();
  markerLine_->Reset();

  DrawPathLine(motion, context);

  // 制御点はスプラインのときだけ意味を持つ
  if (auto *spline = dynamic_cast<const SplineMotion *>(&motion)) {
    DrawControlPoints(*spline, context, selectedPointIndex);
  }

  // 進行マーカー
  if (markerT >= 0.0f) {
    const Vector3 markerPos =
        motion.Evaluate(std::clamp(markerT, 0.0f, 1.0f), context);
    markerLine_->SetColor({1.0f, 0.9f, 0.2f, 1.0f});
    markerLine_->DrawSphere(markerPos, 0.35f, kSphereResolution);
  }

  // Line は1本ごとに独立したバッファなので、それぞれ描画を発行する
  pathLine_->DrawLine();
  pointLine_->DrawLine();
  selectedLine_->DrawLine();
  markerLine_->DrawLine();
}

// ============================================================
// 経路本体を折れ線で描く
// ============================================================
void MotionPathPreview::DrawPathLine(const IAttackMotion &motion,
                                     const MotionContext &context) {
  pathLine_->SetColor({0.4f, 0.9f, 1.0f, 1.0f});

  Vector3 previous = motion.Evaluate(0.0f, context);
  for (int i = 1; i <= kPathSamples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kPathSamples);
    const Vector3 current = motion.Evaluate(t, context);
    pathLine_->RegisterLine(previous, current);
    previous = current;
  }
}

// ============================================================
// 制御点を球で描く
// ============================================================
void MotionPathPreview::DrawControlPoints(const SplineMotion &spline,
                                          const MotionContext &context,
                                          int selectedPointIndex) {
  pointLine_->SetColor({1.0f, 1.0f, 1.0f, 0.8f});
  selectedLine_->SetColor({1.0f, 0.4f, 0.3f, 1.0f});

  for (size_t i = 0; i < spline.points.size(); ++i) {
    const Vector3 world = context.LocalToWorld(spline.points[i], spline.space);
    const bool isSelected = (static_cast<int>(i) == selectedPointIndex);

    // 選択中は一回り大きくして見分けやすくする
    if (isSelected) {
      selectedLine_->DrawSphere(world, 0.32f, kSphereResolution);
    } else {
      pointLine_->DrawSphere(world, 0.22f, kSphereResolution);
    }
  }
}

#endif // USE_IMGUI
