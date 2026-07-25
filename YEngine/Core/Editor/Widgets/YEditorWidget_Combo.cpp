// ===========================================================
// YEditorWidget_Combo.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Combo.h"

namespace YEditorWidget {

bool Combo(const char* label, int& currentIndex,
           std::span<const std::string_view> items)
{
    const std::string_view preview =
        (currentIndex >= 0 && currentIndex < static_cast<int>(items.size()))
            ? items[currentIndex]
            : "";

    ImGui::SetNextItemWidth(-1);
    bool changed = false;
    if (ImGui::BeginCombo(label, preview.data())) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const bool selected = (i == currentIndex);
            if (ImGui::Selectable(items[i].data(), selected)) {
                currentIndex = i;
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool Combo(const char* label, int& currentIndex,
           const char* const* items, int itemCount)
{
    const char* preview =
        (currentIndex >= 0 && currentIndex < itemCount)
            ? items[currentIndex]
            : "";

    ImGui::SetNextItemWidth(-1);
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (int i = 0; i < itemCount; ++i) {
            const bool selected = (i == currentIndex);
            if (ImGui::Selectable(items[i], selected)) {
                currentIndex = i;
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool StringCombo(const char* label, std::string& value,
	std::span<const std::string> items, bool allowEmpty)
{
	const char* preview = value.empty() ? "(なし)" : value.c_str();
	ImGui::SetNextItemWidth(-1);
	bool changed = false;
	if (ImGui::BeginCombo(label, preview)) {
		if (allowEmpty) {
			const bool selected = value.empty();
			if (ImGui::Selectable("(なし)", selected)) {
				value.clear();
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		for (const std::string& item : items) {
			const bool selected = value == item;
			if (ImGui::Selectable(item.c_str(), selected)) {
				value = item;
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
}

} // namespace YEditorWidget
#endif // USE_IMGUI
