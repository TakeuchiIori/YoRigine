// ===========================================================
// YEditorWidget_Toolbar.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Toolbar.h"

namespace YEditorWidget {

namespace {
// 選択中ボタンの色。テーマの ButtonActive を流用してテーマ追従にする。
void PushSelectedStyle() {
  const ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
  ImGui::PushStyleColor(ImGuiCol_Button, active);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active);
}
} // namespace

bool ToolbarSelector(const char *id, const char *const *labels, int count,
                     int &selected, const char *const *tooltips) {
  bool changed = false;

  ImGui::PushID(id);
  for (int i = 0; i < count; ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }

    const bool isSelected = (selected == i);
    if (isSelected) {
      PushSelectedStyle();
    }

    if (ImGui::Button(labels[i])) {
      if (!isSelected) {
        selected = i;
        changed = true;
      }
    }

    if (isSelected) {
      ImGui::PopStyleColor(2);
    }

    if (tooltips && tooltips[i] && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltips[i]);
    }
  }
  ImGui::PopID();

  return changed;
}

bool ToolbarToggle(const char *label, bool &value, const char *tooltip) {
  bool changed = false;

  if (value) {
    PushSelectedStyle();
  }
  if (ImGui::Button(label)) {
    value = !value;
    changed = true;
  }
  if (value) {
    ImGui::PopStyleColor(2);
  }

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return changed;
}

void ToolbarSeparator() {
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
}

} // namespace YEditorWidget
#endif // USE_IMGUI
