#pragma once

#ifdef USE_IMGUI

#include "IAttackMotion.h"
#include "SplineMotion.h"

#include <memory>

namespace YoRigine {
class Camera;
class Line;
} // namespace YoRigine

// ============================================================
// 経路の3Dプレビュー描画
//
// 制御点の数値だけ見ても経路の形は分からないので、
// ゲームビューに線と点で出す。
//   ・経路本体   … 白い線
//   ・制御点     … 球。選択中は色を変える
//   ・進行マーカー … 再生位置を示す球
//   ・基準点     … 経路の原点（自分 or 相手）
//
// 描画だけを担当し、編集の状態は持たない。
// ============================================================
class MotionPathPreview {
public:
  void Initialize(YoRigine::Camera *camera);

  /// <summary>
  /// 経路を描く
  /// </summary>
  /// <param name="motion">描く経路</param>
  /// <param name="context">評価に使う基準情報</param>
  /// <param name="selectedPointIndex">強調表示する制御点（-1で無し）</param>
  /// <param name="markerT">進行マーカーの位置（0〜1、負で非表示）</param>
  void Draw(const IAttackMotion &motion, const MotionContext &context,
            int selectedPointIndex, float markerT);

private:
  // 経路をサンプリングして折れ線で描く
  void DrawPathLine(const IAttackMotion &motion, const MotionContext &context);

  // 制御点を球で描く（スプラインのときだけ）
  void DrawControlPoints(const SplineMotion &spline,
                         const MotionContext &context, int selectedPointIndex);

private:
  YoRigine::Camera *camera_ = nullptr;

  // Line は1本ごとに GPU バッファを持つので、用途別に分ける。
  // 1本を使い回すと後の描画が前の内容を上書きしてしまう。
  std::unique_ptr<YoRigine::Line> pathLine_;
  std::unique_ptr<YoRigine::Line> pointLine_;
  std::unique_ptr<YoRigine::Line> selectedLine_;
  std::unique_ptr<YoRigine::Line> markerLine_;
};

#endif // USE_IMGUI
