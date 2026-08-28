#pragma once
// ===========================================================
// YEditorWidget_ItemWidth.h  (header-only)
//
// ラベルが右へはみ出して読めなくなるのを防ぐための共通ヘルパー。
//
// ImGui はラベルをウィジェットの右側へ描く。ウィジェット幅を -1（目一杯）に
// すると、ラベルは常にウィンドウの外側へ押し出されて見切れてしまう。
// パネルをドッキングして幅を詰めたときに「何の項目か分からない」状態になるため、
// ウィジェット側の幅からラベルぶんを差し引く。
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>

namespace YEditorWidget {

// 次のウィジェットの幅を「利用可能幅 − ラベル幅」にする。
// ラベルが空、または "##" 始まり（非表示ラベル）の場合は目一杯まで広げる。
inline void SetNextItemWidthForLabel(const char* label) {
	if (!label || label[0] == '\0' || (label[0] == '#' && label[1] == '#')) {
		ImGui::SetNextItemWidth(-1.0f);
		return;
	}

	// hide_text_after_double_hash = true で、"表示名##ID" の表示部分だけを測る。
	const float labelWidth =
		ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().ItemInnerSpacing.x;
	const float available = ImGui::GetContentRegionAvail().x;

	// 極端に狭いパネルでも操作できるよう、ウィジェット側の最低幅は残す。
	const float minWidth = ImGui::GetFontSize() * 4.0f;
	const float width = available - labelWidth;
	ImGui::SetNextItemWidth(width > minWidth ? width : minWidth);
}

} // namespace YEditorWidget
#endif // USE_IMGUI
