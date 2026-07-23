#pragma once
// ===========================================================
// YEditorWidget_Color.h
//
// カラーエディタウィジェット。
// ColorHDR は VFX/マテリアルで標準的な HDR 対応版（>1.0 で Bloom に乗る）。
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>
#include "MathFunc.h"

namespace YEditorWidget {

// HDR 対応カラーエディタ（rgba, float 精度）
// VFX・マテリアル・ライトなど発光値が必要な場合に使う
bool ColorHDR(const char* label, Vector4& color);

// HDR 対応 rgb のみ（alpha なし）
bool ColorHDR3(const char* label, Vector3& color);

// 通常 0-1 カラー（UI・スプライト用）
bool Color(const char* label, Vector4& color);

} // namespace YEditorWidget
#endif // USE_IMGUI
