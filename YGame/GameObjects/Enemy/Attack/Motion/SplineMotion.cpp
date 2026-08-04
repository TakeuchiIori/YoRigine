#include "SplineMotion.h"

#include <algorithm>
#include <cmath>

namespace {
// 弧長テーブルの分割数。多いほど等速化の精度が上がる。
constexpr int kArcSamples = 64;
} // namespace

// ============================================================
// 制御点を Catmull-Rom で補間する
//
// 端点は自分自身を複製して外側の制御点の代わりにする。
// こうすると始点と終点をきっちり通る。
// ============================================================
Vector3 SplineMotion::SampleLocal(float t) const {
  if (points.empty())
    return {};
  if (points.size() == 1)
    return points[0];

  const int segmentCount = static_cast<int>(points.size()) - 1;
  const float clamped = std::clamp(t, 0.0f, 1.0f);

  // どの区間にいるか
  const float scaled = clamped * static_cast<float>(segmentCount);
  int index = static_cast<int>(scaled);
  index = std::min(index, segmentCount - 1);
  const float localT = scaled - static_cast<float>(index);

  // Catmull-Rom は前後1点ずつ余分に要る。端は複製で補う。
  const int last = static_cast<int>(points.size()) - 1;
  const Vector3 &p0 = points[std::max(index - 1, 0)];
  const Vector3 &p1 = points[index];
  const Vector3 &p2 = points[std::min(index + 1, last)];
  const Vector3 &p3 = points[std::min(index + 2, last)];

  return CatmullRomInterpolation(p0, p1, p2, p3, localT);
}

// ============================================================
// 弧長テーブルの構築
//
// 制御点の間隔がばらばらだと、進行度をそのまま使ったとき
// 密なところで遅く、疎なところで速く見えてしまう。
// 距離を積み上げた表を作って、進行度を距離基準へ読み替える。
// ============================================================
void SplineMotion::EnsureArcTable() const {
  if (arcTableSourceCount_ == points.size() && !arcTable_.empty()) {
    return;
  }

  arcTable_.clear();
  arcTable_.reserve(kArcSamples + 1);

  float total = 0.0f;
  Vector3 previous = SampleLocal(0.0f);
  arcTable_.push_back(0.0f);

  for (int i = 1; i <= kArcSamples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kArcSamples);
    const Vector3 current = SampleLocal(t);
    total += Length(current - previous);
    arcTable_.push_back(total);
    previous = current;
  }

  // 0〜1 に正規化しておく
  if (total > 0.0001f) {
    for (float &value : arcTable_) {
      value /= total;
    }
  }
  arcTableSourceCount_ = points.size();
}

// ============================================================
// 進行度を距離基準へ読み替える
// ============================================================
float SplineMotion::RemapByArcLength(float t) const {
  EnsureArcTable();
  if (arcTable_.size() < 2)
    return t;

  const float target = std::clamp(t, 0.0f, 1.0f);

  // 目的の距離割合を超える最初の位置を探す
  for (size_t i = 1; i < arcTable_.size(); ++i) {
    if (arcTable_[i] < target)
      continue;

    const float prevArc = arcTable_[i - 1];
    const float currArc = arcTable_[i];
    const float span = currArc - prevArc;

    // 区間内での比率を求めて、その分だけパラメータを進める
    const float ratio = (span > 0.0001f) ? (target - prevArc) / span : 0.0f;
    const float step = 1.0f / static_cast<float>(kArcSamples);
    return (static_cast<float>(i - 1) + ratio) * step;
  }
  return 1.0f;
}

// ============================================================
// 評価
// ============================================================
Vector3 SplineMotion::Evaluate(float t, const MotionContext &ctx) const {
  if (points.empty()) {
    return ctx.startPosition;
  }

  const float sampleT =
      constantSpeed ? RemapByArcLength(t) : std::clamp(t, 0.0f, 1.0f);
  const Vector3 local = SampleLocal(sampleT);
  return ctx.LocalToWorld(local, space);
}

std::unique_ptr<IAttackMotion> SplineMotion::Clone() const {
  auto copy = std::make_unique<SplineMotion>();
  copy->points = points;
  copy->space = space;
  copy->constantSpeed = constantSpeed;
  return copy;
}
