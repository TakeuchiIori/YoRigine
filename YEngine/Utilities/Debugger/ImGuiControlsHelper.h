#pragma once

#ifdef USE_IMGUI
#include "imgui.h"
#endif

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include <string>
#include <cmath>

// =============================================================================
//  ImGuiControlsHelper
//  デバッグエディターで使う UI コントロール群
//  テーマ : "Black Gold" に統一したウィジェット集
//
//  新機能:
//    ・DragXxx / Slider の Gold カラーオーバーライド付きバリアント
//    ・XYZ 軸色分け (X=赤系/Y=緑系/Z=青系) の Vec3 ウィジェット
//    ・GoldCollapsingHeader (ImGui 1.89+ SeparatorText 使用)
//    ・DisabledGroup ヘルパー
// =============================================================================
class ImGuiControlsHelper
{
public:
#ifdef USE_IMGUI

	// =========================================================================
	//  基本スライダー (リセット付き)
	// =========================================================================
	static bool DragFloatWithReset(const char* label, float* value,
		float speed, float min, float max,
		float defaultValue, const char* format = "%.3f");

	static bool DragFloat3WithReset(const char* label, Vector3* value,
		float speed, float min, float max,
		const Vector3& defaultValue, const char* format = "%.3f");

	static bool DragFloat2WithReset(const char* label, Vector2* value,
		float speed, float min, float max,
		const Vector2& defaultValue, const char* format = "%.3f");

	static bool DragIntWithReset(const char* label, int* value,
		float speed, int min, int max, int defaultValue);

	// =========================================================================
	//  入力ボックス (リセット付き)
	// =========================================================================
	static bool InputFloatWithReset(const char* label, float* value,
		float step, float stepFast, float defaultValue, const char* format = "%.3f");

	static bool InputFloat3WithReset(const char* label, Vector3* value,
		const Vector3& defaultValue, const char* format = "%.3f");

	static bool InputFloat2WithReset(const char* label, Vector2* value,
		const Vector2& defaultValue, const char* format = "%.3f");

	static bool InputIntWithReset(const char* label, int* value,
		int step, int stepFast, int defaultValue);

	// =========================================================================
	//  スマートスライダー (スライダー + 入力ボックス 同行)
	// =========================================================================
	static bool SmartSliderFloat(const char* label, float* value,
		float rangeMin, float rangeMax, float defaultValue, const char* format = "%.3f");

	static bool SmartSliderFloat3(const char* label, Vector3* value,
		float rangeMin, float rangeMax,
		const Vector3& defaultValue, const char* format = "%.3f");

	static bool SmartSliderInt(const char* label, int* value,
		int rangeMin, int rangeMax, int defaultValue);

	// =========================================================================
	//  XYZ 軸色分け Vec3 ウィジェット  ← NEW
	//  X = 赤系, Y = 緑系, Z = 青系 でラベルを色分けする見栄え重視のドラッグ
	// =========================================================================
	static bool DragVec3Colored(const char* label, Vector3* value,
		float speed = 0.1f, float min = 0.0f, float max = 0.0f,
		const Vector3& defaultValue = { 0, 0, 0 });

	// =========================================================================
	//  プリセット付き入力
	// =========================================================================
	static bool FloatWithPresets(const char* label, float* value,
		const float* presets, const char** presetNames, int presetCount,
		float defaultValue);

	static bool Vector3WithPresets(const char* label, Vector3* value,
		const Vector3* presets, const char** presetNames, int presetCount,
		const Vector3& defaultValue);

	// =========================================================================
	//  対数スライダー
	// =========================================================================
	static bool LogSliderFloat(const char* label, float* value,
		float logMin, float logMax, float defaultValue, const char* format = "%.3f");

	// =========================================================================
	//  範囲スライダー
	// =========================================================================
	static bool RangeSliderFloat(const char* label,
		float* minValue, float* maxValue,
		float rangeMin, float rangeMax,
		float defaultMin, float defaultMax);

	static bool RangeInputFloat(const char* label,
		float* minValue, float* maxValue,
		float defaultMin, float defaultMax, const char* format = "%.3f");

	// =========================================================================
	//  専用コントロール
	// =========================================================================
	static bool AngleDegrees(const char* label, float* value, float defaultValue);
	static bool AngleRadians(const char* label, float* value, float defaultValue);
	static bool PercentageSlider(const char* label, float* value, float defaultValue);
	static bool TimeInput(const char* label, float* value, float defaultValue);

	// =========================================================================
	//  ベクトル専用コントロール
	// =========================================================================
	static bool DirectionVector(const char* label, Vector3* direction,
		const Vector3& defaultDirection);
	static bool PositionVector(const char* label, Vector3* position,
		const Vector3& defaultPosition);
	static bool ScaleVector(const char* label, Vector3* scale,
		const Vector3& defaultScale);

	// =========================================================================
	//  色制御
	// =========================================================================
	static bool ColorEdit4WithReset(const char* label, Vector4* color,
		const Vector4& defaultValue);
	static bool ColorPicker4WithReset(const char* label, Vector4* color,
		const Vector4& defaultValue);
	static bool ColorPresets(const char* label, Vector4* color,
		const Vector4& defaultValue);

	// =========================================================================
	//  チェックボックス
	// =========================================================================
	static bool CheckboxWithReset(const char* label, bool* value, bool defaultValue);

	// =========================================================================
	//  "Black Gold" テーマ ヘルパー  ← NEW
	// =========================================================================

	// ゴールド色 CollapsingHeader (SeparatorText ベース)
	static bool GoldCollapsingHeader(const char* label,
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen);

	// Disabled グループ (灰色で操作不能)
	static void BeginDisabledGroup(bool disabled = true);
	static void EndDisabledGroup();

	// =========================================================================
	//  ヘルパー
	// =========================================================================
	static void ShowTooltip(const char* text);
	static bool ShowResetButton(const char* id, bool* changed);
	static std::string GetUniqueID(const char* baseLabel);

private:
	static float ConvertToLog(float value, float logMin, float logMax);
	static float ConvertFromLog(float logValue, float logMin, float logMax);

#endif // USE_IMGUI
};