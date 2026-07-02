#include "ImGuiControlsHelper.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdarg>

#ifdef USE_IMGUI
#include <IconsFontAwesome5.h>

// ─────────────────────────────────────────────────────────────────────────────
//  内部カラー定数
// ─────────────────────────────────────────────────────────────────────────────
namespace CtrlTheme
{
	// Black Gold アクセント
	static inline ImVec4 Gold() { return ImVec4(0.852f, 0.682f, 0.196f, 1.00f); }
	static inline ImVec4 GoldBg() { return ImVec4(0.852f, 0.682f, 0.196f, 0.16f); }
	static inline ImVec4 GoldHov() { return ImVec4(0.852f, 0.682f, 0.196f, 0.38f); }
	static inline ImVec4 GoldAct() { return ImVec4(0.852f, 0.682f, 0.196f, 0.60f); }

	// XYZ 軸色
	static inline ImVec4 AxisX() { return ImVec4(0.95f, 0.30f, 0.25f, 1.00f); } // 赤
	static inline ImVec4 AxisY() { return ImVec4(0.35f, 0.85f, 0.35f, 1.00f); } // 緑
	static inline ImVec4 AxisZ() { return ImVec4(0.25f, 0.55f, 0.95f, 1.00f); } // 青
}

// ─────────────────────────────────────────────────────────────────────────────
//  内部マクロ: ゴールドカラーリセットボタンを右端に配置
// ─────────────────────────────────────────────────────────────────────────────
#define GOLD_RESET_BUTTON(id, changed)                                 \
	do {                                                                \
		ImGui::SameLine();                                              \
		ImGui::PushStyleColor(ImGuiCol_Button,        CtrlTheme::GoldBg()); \
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());\
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  CtrlTheme::GoldAct());\
		ImGui::PushStyleColor(ImGuiCol_Text,          CtrlTheme::Gold());   \
		if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + (id)).c_str())) \
		{ (changed) = true; ImGui::PopStyleColor(4); return true; }     \
		ImGui::PopStyleColor(4);                                        \
	} while (false)

// =============================================================================
//  基本スライダー (リセット付き)
// =============================================================================

bool ImGuiControlsHelper::DragFloatWithReset(
	const char* label, float* value,
	float speed, float min, float max,
	float defaultValue, const char* format)
{
	bool changed = ImGui::DragFloat(label, value, speed, min, max,
		format, ImGuiSliderFlags_AlwaysClamp);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}


bool ImGuiControlsHelper::DragFloat3WithReset(
	const char* label, Vector3* value,
	float speed, float min, float max,
	const Vector3& defaultValue, const char* format)
{
	bool changed = ImGui::DragFloat3(label, &value->x, speed, min, max,
		format, ImGuiSliderFlags_AlwaysClamp);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::DragFloat2WithReset(
	const char* label, Vector2* value,
	float speed, float min, float max,
	const Vector2& defaultValue, const char* format)
{
	bool changed = ImGui::DragFloat2(label, &value->x, speed, min, max,
		format, ImGuiSliderFlags_AlwaysClamp);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::DragIntWithReset(
	const char* label, int* value,
	float speed, int min, int max, int defaultValue)
{
	bool changed = ImGui::DragInt(label, value, speed, min, max);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  入力ボックス (リセット付き)
// =============================================================================

bool ImGuiControlsHelper::InputFloatWithReset(
	const char* label, float* value,
	float step, float stepFast, float defaultValue, const char* format)
{
	bool changed = ImGui::InputFloat(label, value, step, stepFast, format);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::InputFloat3WithReset(
	const char* label, Vector3* value,
	const Vector3& defaultValue, const char* format)
{
	bool changed = ImGui::InputFloat3(label, &value->x, format);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::InputFloat2WithReset(
	const char* label, Vector2* value,
	const Vector2& defaultValue, const char* format)
{
	bool changed = ImGui::InputFloat2(label, &value->x, format);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::InputIntWithReset(
	const char* label, int* value,
	int step, int stepFast, int defaultValue)
{
	bool changed = ImGui::InputInt(label, value, step, stepFast);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  スマートスライダー
// =============================================================================

bool ImGuiControlsHelper::SmartSliderFloat(
	const char* label, float* value,
	float rangeMin, float rangeMax, float defaultValue, const char* format)
{
	bool changed = false;
	float avail = ImGui::GetContentRegionAvail().x;

	// スライダー
	ImGui::PushStyleColor(ImGuiCol_SliderGrab, CtrlTheme::Gold());
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CtrlTheme::GoldAct());
	ImGui::PushItemWidth(avail * 0.50f);
	if (ImGui::SliderFloat(("##sl_" + std::string(label)).c_str(),
		value, rangeMin, rangeMax, format)) {
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopStyleColor(2);

	// 入力ボックス
	ImGui::SameLine(0, 4);
	ImGui::PushItemWidth(avail * 0.22f);
	if (ImGui::InputFloat(("##in_" + std::string(label)).c_str(),
		value, 0.0f, 0.0f, format)) {
		changed = true;
	}
	ImGui::PopItemWidth();

	// ラベル
	ImGui::SameLine(0, 6);
	ImGui::TextUnformatted(label);

	// リセット
	std::string uid = GetUniqueID(label);
	ImGui::SameLine(0, 4);
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::SmartSliderFloat3(
	const char* label, Vector3* value,
	float rangeMin, float rangeMax,
	const Vector3& defaultValue, const char* format)
{
	bool changed = false;
	ImGui::TextUnformatted(label);

	ImGui::PushID("smsl3x");
	if (SmartSliderFloat("X", &value->x, rangeMin, rangeMax, defaultValue.x, format)) changed = true;
	ImGui::PopID();

	ImGui::PushID("smsl3y");
	if (SmartSliderFloat("Y", &value->y, rangeMin, rangeMax, defaultValue.y, format)) changed = true;
	ImGui::PopID();

	ImGui::PushID("smsl3z");
	if (SmartSliderFloat("Z", &value->z, rangeMin, rangeMax, defaultValue.z, format)) changed = true;
	ImGui::PopID();

	return changed;
}

bool ImGuiControlsHelper::SmartSliderInt(
	const char* label, int* value,
	int rangeMin, int rangeMax, int defaultValue)
{
	bool changed = false;
	float avail = ImGui::GetContentRegionAvail().x;

	ImGui::PushStyleColor(ImGuiCol_SliderGrab, CtrlTheme::Gold());
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CtrlTheme::GoldAct());
	ImGui::PushItemWidth(avail * 0.50f);
	if (ImGui::SliderInt(("##sl_" + std::string(label)).c_str(),
		value, rangeMin, rangeMax)) {
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopStyleColor(2);

	ImGui::SameLine(0, 4);
	ImGui::PushItemWidth(avail * 0.22f);
	if (ImGui::InputInt(("##in_" + std::string(label)).c_str(), value)) {
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::SameLine(0, 6);
	ImGui::TextUnformatted(label);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine(0, 4);
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  XYZ 軸色分け Vec3 ドラッグ  ← NEW
// =============================================================================
bool ImGuiControlsHelper::DragVec3Colored(
	const char* label, Vector3* value,
	float speed, float min, float max,
	const Vector3& defaultValue)
{
	bool changed = false;

	// ラベル行
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	ImGui::TextUnformatted(label);
	ImGui::PopStyleColor();

	float totalW = ImGui::GetContentRegionAvail().x;
	float labelW = 14.0f;  // "X" "Y" "Z" ラベル幅
	float resetW = 22.0f;  // リセットボタン幅
	float spacing = 4.0f;
	float dragW = (totalW - (labelW + spacing) * 3 - resetW - spacing * 2) / 3.0f;
	if (dragW < 40.0f) dragW = 40.0f;

	ImGui::PushID(label);

	// ----- X -----
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::AxisX());
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25f, 0.06f, 0.05f, 0.80f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.40f, 0.10f, 0.08f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.55f, 0.15f, 0.12f, 1.00f));
	ImGui::Text("X");
	ImGui::SameLine(0, 2);
	ImGui::PushItemWidth(dragW);
	if (ImGui::DragFloat("##X", &value->x, speed, min, max, "%.3f",
		ImGuiSliderFlags_AlwaysClamp)) changed = true;
	ImGui::PopItemWidth();
	ImGui::PopStyleColor(4);

	// ----- Y -----
	ImGui::SameLine(0, spacing);
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::AxisY());
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.22f, 0.06f, 0.80f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.09f, 0.35f, 0.09f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.48f, 0.12f, 1.00f));
	ImGui::Text("Y");
	ImGui::SameLine(0, 2);
	ImGui::PushItemWidth(dragW);
	if (ImGui::DragFloat("##Y", &value->y, speed, min, max, "%.3f",
		ImGuiSliderFlags_AlwaysClamp)) changed = true;
	ImGui::PopItemWidth();
	ImGui::PopStyleColor(4);

	// ----- Z -----
	ImGui::SameLine(0, spacing);
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::AxisZ());
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.10f, 0.28f, 0.80f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.09f, 0.15f, 0.42f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.20f, 0.56f, 1.00f));
	ImGui::Text("Z");
	ImGui::SameLine(0, 2);
	ImGui::PushItemWidth(dragW);
	if (ImGui::DragFloat("##Z", &value->z, speed, min, max, "%.3f",
		ImGuiSliderFlags_AlwaysClamp)) changed = true;
	ImGui::PopItemWidth();
	ImGui::PopStyleColor(4);

	// ----- リセット -----
	ImGui::SameLine(0, spacing);
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton(ICON_FA_UNDO "##reset")) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	ImGui::PopID();
	return changed;
}

// =============================================================================
//  プリセット付き入力
// =============================================================================

bool ImGuiControlsHelper::FloatWithPresets(
	const char* label, float* value,
	const float* presets, const char** presetNames, int presetCount,
	float defaultValue)
{
	bool changed = false;
	if (InputFloatWithReset(label, value, 0.1f, 1.0f, defaultValue)) changed = true;

	if (presetCount > 0) {
		ImGui::TextDisabled("プリセット:");
		ImGui::SameLine();

		const int kPerRow = 4;
		for (int i = 0; i < presetCount; ++i) {
			ImGui::PushID(i);
			ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
			ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
			if (ImGui::SmallButton(presetNames[i])) {
				*value = presets[i];
				changed = true;
			}
			ImGui::PopStyleColor(3);
			ImGui::PopID();

			if ((i + 1) % kPerRow != 0 && i < presetCount - 1) ImGui::SameLine();
		}
	}
	return changed;
}

bool ImGuiControlsHelper::Vector3WithPresets(
	const char* label, Vector3* value,
	const Vector3* presets, const char** presetNames, int presetCount,
	const Vector3& defaultValue)
{
	bool changed = false;
	if (InputFloat3WithReset(label, value, defaultValue)) changed = true;

	if (presetCount > 0) {
		ImGui::TextDisabled("プリセット:");
		ImGui::SameLine();

		const int kPerRow = 4;
		for (int i = 0; i < presetCount; ++i) {
			ImGui::PushID(i);
			ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
			ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
			if (ImGui::SmallButton(presetNames[i])) {
				*value = presets[i];
				changed = true;
			}
			ImGui::PopStyleColor(3);
			ImGui::PopID();

			if ((i + 1) % kPerRow != 0 && i < presetCount - 1) ImGui::SameLine();
		}
	}
	return changed;
}

// =============================================================================
//  対数スライダー
// =============================================================================

bool ImGuiControlsHelper::LogSliderFloat(
	const char* label, float* value,
	float logMin, float logMax, float defaultValue, const char* format)
{
	float logVal = ConvertToLog(*value, logMin, logMax);

	ImGui::PushStyleColor(ImGuiCol_SliderGrab, CtrlTheme::Gold());
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CtrlTheme::GoldAct());
	bool changed = ImGui::SliderFloat(label, &logVal, 0.0f, 1.0f, "対数");
	ImGui::PopStyleColor(2);

	if (changed) {
		*value = ConvertFromLog(logVal, logMin, logMax);
	}

	ImGui::SameLine();
	ImGui::Text(format, *value);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  範囲スライダー
// =============================================================================

bool ImGuiControlsHelper::RangeSliderFloat(
	const char* label,
	float* minValue, float* maxValue,
	float rangeMin, float rangeMax,
	float defaultMin, float defaultMax)
{
	bool changed = false;
	ImGui::TextUnformatted(label);

	ImGui::PushStyleColor(ImGuiCol_SliderGrab, CtrlTheme::Gold());
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, CtrlTheme::GoldAct());

	float avail = ImGui::GetContentRegionAvail().x * 0.45f;
	ImGui::PushItemWidth(avail);
	if (ImGui::SliderFloat(("最小##" + std::string(label)).c_str(),
		minValue, rangeMin, rangeMax, "%.2f")) {
		if (*minValue > *maxValue) *maxValue = *minValue;
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::SliderFloat(("最大##" + std::string(label)).c_str(),
		maxValue, rangeMin, rangeMax, "%.2f")) {
		if (*maxValue < *minValue) *minValue = *maxValue;
		changed = true;
	}
	ImGui::PopItemWidth();
	ImGui::PopStyleColor(2);

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##rng") + label).c_str())) {
		*minValue = defaultMin;
		*maxValue = defaultMax;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::RangeInputFloat(
	const char* label,
	float* minValue, float* maxValue,
	float defaultMin, float defaultMax, const char* format)
{
	bool changed = false;
	ImGui::TextUnformatted(label);

	float avail = ImGui::GetContentRegionAvail().x * 0.38f;
	ImGui::PushItemWidth(avail);
	if (ImGui::InputFloat(("最小##" + std::string(label)).c_str(),
		minValue, 0, 0, format)) {
		if (*minValue > *maxValue) *maxValue = *minValue;
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::InputFloat(("最大##" + std::string(label)).c_str(),
		maxValue, 0, 0, format)) {
		if (*maxValue < *minValue) *minValue = *maxValue;
		changed = true;
	}
	ImGui::PopItemWidth();

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##rngi") + label).c_str())) {
		*minValue = defaultMin;
		*maxValue = defaultMax;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  専用コントロール
// =============================================================================

bool ImGuiControlsHelper::AngleDegrees(
	const char* label, float* value, float defaultValue)
{
	float deg = *value * 180.0f / 3.14159265359f;
	float defaultDeg = defaultValue * 180.0f / 3.14159265359f;

	bool changed = DragFloatWithReset(label, &deg, 1.0f, -360.0f, 360.0f, defaultDeg, "%.1f\xc2\xb0");
	if (changed) *value = deg * 3.14159265359f / 180.0f;
	return changed;
}

bool ImGuiControlsHelper::AngleRadians(
	const char* label, float* value, float defaultValue)
{
	return DragFloatWithReset(label, value,
		0.01f, -6.28318530718f, 6.28318530718f, defaultValue, "%.3f rad");
}

bool ImGuiControlsHelper::PercentageSlider(
	const char* label, float* value, float defaultValue)
{
	float pct = *value * 100.0f;
	float defPct = defaultValue * 100.0f;

	bool changed = DragFloatWithReset(label, &pct, 1.0f, 0.0f, 100.0f, defPct, "%.1f%%");
	if (changed) *value = pct / 100.0f;
	return changed;
}

bool ImGuiControlsHelper::TimeInput(
	const char* label, float* value, float defaultValue)
{
	bool changed = DragFloatWithReset(label, value, 0.1f, 0.0f, 3600.0f, defaultValue, "%.1f s");

	if (*value >= 60.0f) {
		ImGui::SameLine();
		ImGui::TextDisabled(*value >= 3600.0f
			? "(%.1f h)" : "(%.1f m)",
			*value >= 3600.0f ? *value / 3600.0f : *value / 60.0f);
	}

	return changed;
}

// =============================================================================
//  ベクトル専用コントロール
// =============================================================================

bool ImGuiControlsHelper::DirectionVector(
	const char* label, Vector3* direction, const Vector3& defaultDirection)
{
	bool changed = false;

	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	ImGui::TextUnformatted(label);
	ImGui::PopStyleColor();

	if (DragVec3Colored("方向##dir", direction, 0.01f, -1.0f, 1.0f, defaultDirection)) {
		float len = std::sqrt(
			direction->x * direction->x +
			direction->y * direction->y +
			direction->z * direction->z);
		if (len > 0.001f) {
			direction->x /= len;
			direction->y /= len;
			direction->z /= len;
		}
		else {
			*direction = defaultDirection;
		}
		changed = true;
	}

	// プリセット方向ボタン (ゴールド)
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());

	auto presetBtn = [&](const char* name, Vector3 v) {
		if (ImGui::SmallButton(name)) { *direction = v; changed = true; }
		};

	presetBtn("+Y", { 0, 1, 0 }); ImGui::SameLine();
	presetBtn("-Y", { 0,-1, 0 }); ImGui::SameLine();
	presetBtn("+X", { 1, 0, 0 }); ImGui::SameLine();
	presetBtn("-X", { -1, 0, 0 }); ImGui::SameLine();
	presetBtn("+Z", { 0, 0, 1 }); ImGui::SameLine();
	presetBtn("-Z", { 0, 0,-1 });

	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::PositionVector(
	const char* label, Vector3* position, const Vector3& defaultPosition)
{
	return DragVec3Colored(label, position, 0.1f, 0.0f, 0.0f, defaultPosition);
}

bool ImGuiControlsHelper::ScaleVector(
	const char* label, Vector3* scale, const Vector3& defaultScale)
{
	bool changed = DragVec3Colored(label, scale, 0.01f, 0.001f, 100.0f, defaultScale);

	// 統一スケールボタン
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_ARROWS_ALT "##uni") + label).c_str())) {
		float avg = (scale->x + scale->y + scale->z) / 3.0f;
		scale->x = scale->y = scale->z = avg;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	if (ImGui::BeginItemTooltip()) { // 1.91+
		ImGui::TextUnformatted("XYZ 平均で統一スケール");
		ImGui::EndTooltip();
	}

	return changed;
}

// =============================================================================
//  色制御
// =============================================================================

bool ImGuiControlsHelper::ColorEdit4WithReset(
	const char* label, Vector4* color, const Vector4& defaultValue)
{
	bool changed = ImGui::ColorEdit4(label, &color->x,
		ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*color = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::ColorPicker4WithReset(
	const char* label, Vector4* color, const Vector4& defaultValue)
{
	bool changed = ImGui::ColorPicker4(label, &color->x,
		ImGuiColorEditFlags_AlphaBar);

	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::Button((std::string(ICON_FA_UNDO "  リセット##") + label).c_str())) {
		*color = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

bool ImGuiControlsHelper::ColorPresets(
	const char* label, Vector4* color, const Vector4& defaultValue)
{
	bool changed = false;

	if (ImGui::ColorEdit4(label, &color->x,
		ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
		changed = true;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	ImGui::TextUnformatted("プリセット:");
	ImGui::PopStyleColor();

	struct Preset { Vector4 col; const char* name; };
	static const Preset kPresets[] = {
		{{ 1.0f, 1.0f, 1.0f, 1.0f }, "白"   },
		{{ 1.0f, 0.0f, 0.0f, 1.0f }, "赤"   },
		{{ 0.0f, 1.0f, 0.0f, 1.0f }, "緑"   },
		{{ 0.0f, 0.0f, 1.0f, 1.0f }, "青"   },
		{{ 1.0f, 1.0f, 0.0f, 1.0f }, "黄"   },
		{{ 1.0f, 0.0f, 1.0f, 1.0f }, "紫"   },
		{{ 0.0f, 1.0f, 1.0f, 1.0f }, "水色" },
		{{ 0.85f,0.68f,0.20f,1.0f  }, "金"  },
		{{ 0.0f, 0.0f, 0.0f, 1.0f }, "黒"   },
	};

	for (int i = 0; i < (int)(sizeof(kPresets) / sizeof(kPresets[0])); ++i) {
		ImGui::PushID(i);
		if (ImGui::ColorButton(kPresets[i].name,
			ImVec4(kPresets[i].col.x, kPresets[i].col.y,
				kPresets[i].col.z, kPresets[i].col.w),
			0, ImVec2(22, 22))) {
			*color = kPresets[i].col;
			changed = true;
		}
		if (ImGui::BeginItemTooltip()) {   // 1.91+
			ImGui::TextUnformatted(kPresets[i].name);
			ImGui::EndTooltip();
		}
		if (i % 5 != 4) ImGui::SameLine();
		ImGui::PopID();
	}

	// リセット
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##cprst") + label).c_str())) {
		*color = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  チェックボックス
// =============================================================================

bool ImGuiControlsHelper::CheckboxWithReset(
	const char* label, bool* value, bool defaultValue)
{
	ImGui::PushStyleColor(ImGuiCol_CheckMark, CtrlTheme::Gold());
	bool changed = ImGui::Checkbox(label, value);
	ImGui::PopStyleColor();

	std::string uid = GetUniqueID(label);
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	if (ImGui::SmallButton((std::string(ICON_FA_UNDO "##") + uid).c_str())) {
		*value = defaultValue;
		changed = true;
	}
	ImGui::PopStyleColor(4);

	return changed;
}

// =============================================================================
//  "Black Gold" テーマ ヘルパー  ← NEW
// =============================================================================

bool ImGuiControlsHelper::GoldCollapsingHeader(
	const char* label, ImGuiTreeNodeFlags flags)
{
	// CollapsingHeader をゴールドでカラーリング
	ImGui::PushStyleColor(ImGuiCol_Header, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());
	bool open = ImGui::CollapsingHeader(label, flags);
	ImGui::PopStyleColor(4);
	return open;
}

void ImGuiControlsHelper::BeginDisabledGroup(bool disabled)
{
	if (disabled) ImGui::BeginDisabled();
}

void ImGuiControlsHelper::EndDisabledGroup()
{
	// 内部で enabled かどうかを判定するのが面倒なので、
	// BeginDisabledGroup と対で使うことを前提とする
	ImGui::EndDisabled();
}

// =============================================================================
//  ヘルパー
// =============================================================================

void ImGuiControlsHelper::ShowTooltip(const char* text)
{
	if (ImGui::BeginItemTooltip()) {   // 1.91+ API
		ImGui::TextUnformatted(text);
		ImGui::EndTooltip();
	}
}

bool ImGuiControlsHelper::ShowResetButton(const char* id, bool* changed)
{
	ImGui::PushStyleColor(ImGuiCol_Button, CtrlTheme::GoldBg());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CtrlTheme::GoldHov());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, CtrlTheme::GoldAct());
	ImGui::PushStyleColor(ImGuiCol_Text, CtrlTheme::Gold());

	bool clicked = ImGui::SmallButton((std::string(ICON_FA_UNDO "##rst") + id).c_str());
	ImGui::PopStyleColor(4);

	if (clicked && changed) *changed = true;
	return clicked;
}

std::string ImGuiControlsHelper::GetUniqueID(const char* baseLabel)
{
	return std::string("##") + baseLabel;
}

float ImGuiControlsHelper::ConvertToLog(float value, float logMin, float logMax)
{
	if (value <= logMin) return 0.0f;
	if (value >= logMax) return 1.0f;
	return std::log(value / logMin) / std::log(logMax / logMin);
}

float ImGuiControlsHelper::ConvertFromLog(float logValue, float logMin, float logMax)
{
	return logMin * std::pow(logMax / logMin,
		std::max(0.0f, std::min(1.0f, logValue)));
}

#endif // USE_IMGUI