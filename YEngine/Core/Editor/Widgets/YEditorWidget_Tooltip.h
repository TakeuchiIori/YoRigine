#pragma once
// ===========================================================
// YEditorWidget_Tooltip.h  (header-only)
//
// ツールチップ系ユーティリティ。
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>

namespace YEditorWidget {

// 直前のアイテムにホバーしたときツールチップを表示する
inline void ItemTooltip(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", text);
}

// "(?)" ボタン付きヘルプマーカー（ラベルの右端に置く想定）
inline void HelpMarker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

} // namespace YEditorWidget
#endif // USE_IMGUI
