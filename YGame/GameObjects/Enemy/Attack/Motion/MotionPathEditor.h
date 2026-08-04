#pragma once

#ifdef USE_IMGUI

#include "Debugger/Gizmo/GizmoController.h"
#include "MotionPathLibrary.h"
#include "MotionPathPreview.h"

#include <string>

namespace YoRigine {
class Camera;
}

// ============================================================
// 経路エディタ
//
// 攻撃の動きを制御点で作る。数値だけ並べても形が分からないので、
// ゲームビューに経路を線で出し、制御点をギズモで掴めるようにする。
//
// 経路は敵の向きを基準にしたローカル座標で持つため、
// プレビューでは「仮の敵位置と向き」を置いて確認する。
// ============================================================
class MotionPathEditor {
public:
  void Initialize(YoRigine::Camera *camera);

  // ImGui パネル本体
  void Draw();

  // ゲームビューのギズモ描画（Editor のギズモコールバックから呼ぶ）
  void DrawGizmo();

  // 経路の3Dプレビュー（シーンのライン描画から呼ぶ）
  void DrawPreview();

private:
  // ── パネルの各部 ──
  void DrawPathList();
  void DrawPathSettings(MotionPathEntry &entry);
  void DrawSplineEditor(SplineMotion &spline);
  void DrawOrbitEditor(class OrbitMotion &orbit);
  void DrawPreviewSettings();

  // 選択中の経路（無ければ nullptr）
  MotionPathEntry *GetSelectedEntry();

  // プレビュー用の基準情報を組み立てる
  MotionContext MakePreviewContext() const;

private:
  YoRigine::Camera *camera_ = nullptr;

  MotionPathPreview preview_;
  GizmoController gizmo_;

  int selectedPath_ = -1;
  int selectedPoint_ = -1;

  // ── プレビューの仮想的な状況 ──
  // 経路はローカル座標なので、確認には「敵がどこを向いて立っているか」
  // 「相手がどこにいるか」を仮に置く必要がある。
  Vector3 previewOrigin_{0.0f, 0.0f, 0.0f};
  float previewYawDeg_ = 0.0f;
  Vector3 previewTarget_{0.0f, 0.0f, 8.0f};
  float previewScale_ = 1.0f;

  // ── 再生 ──
  bool playing_ = false;
  float playTime_ = 0.0f;
  float playDuration_ = 1.0f;

  char nameBuffer_[128] = {};
  bool initialized_ = false;
};

#endif // USE_IMGUI
