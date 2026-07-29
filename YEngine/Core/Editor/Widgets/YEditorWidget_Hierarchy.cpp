// ===========================================================
// YEditorWidget_Hierarchy.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Hierarchy.h"

namespace YEditorWidget {

namespace {
// アイコンフォントに依存しないよう、記号は ASCII / 全角の範囲で済ませる
constexpr const char *kVisibleOn = "o";
constexpr const char *kVisibleOff = "-";
constexpr const char *kLockOn = "L";
constexpr const char *kLockOff = " ";
constexpr float kIndentWidth = 14.0f;
} // namespace

HierarchyRowResult DrawHierarchyRow(int id, const char *label, bool selected,
                                    const HierarchyRow &row) {
  HierarchyRowResult result;

  ImGui::PushID(id);

  // ── 表示トグル ──
  if (row.visible) {
    if (ImGui::SmallButton(*row.visible ? kVisibleOn : kVisibleOff)) {
      *row.visible = !*row.visible;
      result.toggledVisible = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(*row.visible ? "表示中 (クリックで非表示)"
                                     : "非表示 (クリックで表示)");
    }
    ImGui::SameLine();
  }

  // ── 選択ロックトグル ──
  if (row.locked) {
    if (ImGui::SmallButton(*row.locked ? kLockOn : kLockOff)) {
      *row.locked = !*row.locked;
      result.toggledLock = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(*row.locked
                            ? "ロック中: ビューポートのクリックで選ばれない"
                            : "クリックで選択ロック (Pick から除外)");
    }
    ImGui::SameLine();
  }

  // ── 折りたたみ ──
  if (row.hasChildren && row.expanded) {
    if (ImGui::SmallButton(*row.expanded ? "v" : ">")) {
      *row.expanded = !*row.expanded;
      result.toggledExpand = true;
    }
    ImGui::SameLine();
  }

  // ── インデント + 本体 ──
  if (row.indent > 0) {
    ImGui::Indent(kIndentWidth * static_cast<float>(row.indent));
  }

  if (ImGui::Selectable(label, selected,
                        ImGuiSelectableFlags_AllowDoubleClick)) {
    result.clicked = true;
    result.additive = ImGui::GetIO().KeyCtrl;
    result.doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  }

  if (ImGui::BeginPopupContextItem("##rowMenu")) {
    if (ImGui::MenuItem("名前を変更")) {
      result.requestRename = true;
    }
    if (ImGui::MenuItem("複製")) {
      result.requestDuplicate = true;
    }
    ImGui::Separator();
    if (ImGui::MenuItem("削除")) {
      result.requestDelete = true;
    }
    ImGui::EndPopup();
  }

  if (row.indent > 0) {
    ImGui::Unindent(kIndentWidth * static_cast<float>(row.indent));
  }

  ImGui::PopID();
  return result;
}

} // namespace YEditorWidget
#endif // USE_IMGUI
