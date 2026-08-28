#pragma once
// ===========================================================
// YEditorWidget_Transform.h
//
// トランスフォーム編集ウィジェット。
// 回転は内部ラジアン / 表示は度数で統一する（各エディタで
// DegToRad を書き散らさないための共通化）。
//
// 使い方:
//   YEditorWidget::TransformFields tf;
//   if (tf.Draw(obj.position, obj.rotation, obj.scale)) { /* 変更あり */ }
// ===========================================================
#ifdef USE_IMGUI
#include "Vector3.h"
#include <imgui.h>

namespace YEditorWidget {

// ── トランスフォーム編集ブロック ─────────────────────────────
// 位置 / 回転(度表示) / スケール を 1 セットで編集する。
// スケールの一様編集ロックなど、UI 側の状態をメンバに持つのでクラスにしている。
class TransformFields {
public:
  // 3 項目をまとめて描画する。いずれかが変更されたら true。
  // rotationRadians はラジアンのまま受け取り、度数で表示・編集する。
  bool Draw(Vector3 &position, Vector3 &rotationRadians, Vector3 &scale);

  // 位置 / 回転 / スケールのリセットボタン列。押されたら true。
  bool DrawResetButtons(Vector3 &position, Vector3 &rotationRadians,
                        Vector3 &scale);

  // スケールの一様編集（Unity のリンクアイコン相当）
  void SetUniformScale(bool uniform) { uniformScale_ = uniform; }
  bool IsUniformScale() const { return uniformScale_; }

  // ドラッグ速度の調整
  void SetPositionSpeed(float speed) { positionSpeed_ = speed; }
  void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
  void SetScaleSpeed(float speed) { scaleSpeed_ = speed; }

private:
  bool DrawScale(Vector3 &scale);

  bool uniformScale_ = false;
  float positionSpeed_ = 0.1f;
  float rotationSpeed_ = 1.0f;
  float scaleSpeed_ = 0.01f;
};

// ── 単発ヘルパー ─────────────────────────────────────────────
// ラジアン値を度数で編集する DragFloat3。
bool DragEulerDegrees(const char *label, Vector3 &radians, float speed = 1.0f);

} // namespace YEditorWidget
#endif // USE_IMGUI
