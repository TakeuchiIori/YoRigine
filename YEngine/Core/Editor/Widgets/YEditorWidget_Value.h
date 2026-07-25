#pragma once
// ===========================================================
// YEditorWidget_Value.h
//
// 数値・ベクトル入力ウィジェット。
// 型安全ラッパー（float& / Vector3&）。幅は ImGui デフォルト（ラベル右表示）。
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>
#include "MathFunc.h"

namespace YEditorWidget {

// ── Float ───────────────────────────────────────────────────
bool DragFloat(const char* label, float& v,
               float speed = 0.01f, float vmin = 0.f, float vmax = 0.f,
               const char* fmt = "%.3f");

bool SliderFloat(const char* label, float& v, float vmin, float vmax,
                 const char* fmt = "%.3f");

// 度数表示の角度スライダー（内部はラジアン）
bool AngleSlider(const char* label, float& radians,
                 float minDeg = -180.f, float maxDeg = 180.f);

// ── Int ─────────────────────────────────────────────────────
bool DragInt(const char* label, int& v,
             float speed = 1.f, int vmin = 0, int vmax = 0);

bool SliderInt(const char* label, int& v, int vmin, int vmax);

// ── Vector ──────────────────────────────────────────────────
bool DragVec2(const char* label, Vector2& v,
              float speed = 0.01f, float vmin = 0.f, float vmax = 0.f);

bool DragVec3(const char* label, Vector3& v,
              float speed = 0.01f, float vmin = 0.f, float vmax = 0.f,
              const char* fmt = "%.3f");

// DragVec3 + ボタン1つで正規化（Direction パラメータ向け）
bool DirectionVec3(const char* label, Vector3& dir, float speed = 0.01f);

// min/max をセットで編集（寿命範囲・速度範囲など）
bool RangeFloat(const char* label, float& vmin, float& vmax,
                float speed = 0.01f);

} // namespace YEditorWidget
#endif // USE_IMGUI
