#ifdef USE_IMGUI

#include "OutlinerPanel.h"

// Engine
#include "../Edit/ScenePlacementService.h"
#include "../ObjectSelector.h"

// C++
#include <algorithm>
#include <cstdio>

namespace YoRigine {

namespace {

// ObjectManager の内部は unordered_map なので、そのまま並べると
// フレームごとに行の順番が変わってしまう。必ず ID 昇順に整列する。
std::vector<ObjectManager::PlacedObject *>
CollectSorted(ObjectManager &objectManager) {
  auto objects = objectManager.GetAllActiveObjects();
  std::sort(objects.begin(), objects.end(),
            [](const ObjectManager::PlacedObject *a,
               const ObjectManager::PlacedObject *b) { return a->id < b->id; });
  return objects;
}

// 一覧に出す表示名。nameTag があればそれを優先する。
std::string BuildLabel(const ObjectManager::PlacedObject &obj) {
  if (!obj.nameTag.empty()) {
    return obj.nameTag + "  [" + obj.modelName + "]";
  }
  return obj.modelName + " (" + std::to_string(obj.id) + ")";
}

} // namespace

bool OutlinerPanel::IsCollapsed(int id) const {
  return std::find(collapsedIds_.begin(), collapsedIds_.end(), id) !=
         collapsedIds_.end();
}

void OutlinerPanel::SetCollapsed(int id, bool collapsed) {
  const auto it = std::find(collapsedIds_.begin(), collapsedIds_.end(), id);
  if (collapsed && it == collapsedIds_.end()) {
    collapsedIds_.push_back(id);
  } else if (!collapsed && it != collapsedIds_.end()) {
    collapsedIds_.erase(it);
  }
}

//=============================================================================
// 1 行
//=============================================================================
void OutlinerPanel::DrawRow(const ScenePanelContext &context,
                            ObjectManager::PlacedObject &obj, int depth) {
  ObjectManager &objectManager = *context.scene->objectManager;
  ObjectSelector &selector = *context.scene->selector;

  // 名前変更中はインライン入力に差し替える
  if (renamingId_ == obj.id) {
    ImGui::PushID(obj.id);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##rename", renameBuffer_, sizeof(renameBuffer_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      obj.nameTag = renameBuffer_;
      renamingId_ = -1;
    }
    if (ImGui::IsItemDeactivated()) {
      renamingId_ = -1;
    }
    ImGui::SetKeyboardFocusHere(-1);
    ImGui::PopID();
    return;
  }

  const auto children = objectManager.GetChildObjects(obj.id);

  YEditorWidget::HierarchyRow row;
  row.visible = &obj.visible;
  // ロック = ピック対象から外す。UI 上は pickable の反転として扱う。
  bool locked = !obj.pickable;
  row.locked = &locked;
  row.indent = depth;
  row.hasChildren = !children.empty();
  bool expanded = !IsCollapsed(obj.id);
  row.expanded = &expanded;

  const std::string label = BuildLabel(obj);
  const auto result = YEditorWidget::DrawHierarchyRow(
      obj.id, label.c_str(), selector.IsSelected(obj.id), row);

  if (result.toggledLock) {
    obj.pickable = !locked;
  }
  if (result.toggledExpand) {
    SetCollapsed(obj.id, !expanded);
  }
  if (result.clicked) {
    selector.SelectObject(obj.id, result.additive);
  }
  if (result.requestRename) {
    renamingId_ = obj.id;
    std::snprintf(renameBuffer_, sizeof(renameBuffer_), "%s",
                  obj.nameTag.c_str());
  }
  if (result.requestDuplicate) {
    objectManager.DuplicateObject(obj.id, {1.0f, 0.0f, 0.0f});
  }
  if (result.requestDelete) {
    objectManager.DeleteObject(obj.id);
    selector.RemoveFromSelection(obj.id);
    return; // 削除したオブジェクトの子は次フレームで処理される
  }

  if (!expanded) {
    return;
  }
  for (auto *child : children) {
    if (child) {
      DrawRow(context, *child, depth + 1);
    }
  }
}

//=============================================================================
// 検索中: 階層を無視したフラット表示
//=============================================================================
void OutlinerPanel::DrawFlatList(const ScenePanelContext &context) {
  for (auto *obj : CollectSorted(*context.scene->objectManager)) {
    if (!obj) {
      continue;
    }
    const std::string label = BuildLabel(*obj);
    if (!searchBox_.Matches(label)) {
      continue;
    }
    DrawRow(context, *obj, 0);
  }
}

//=============================================================================
// 通常時: 親子関係に沿ったツリー表示
//=============================================================================
void OutlinerPanel::DrawHierarchy(const ScenePanelContext &context) {
  for (auto *obj : CollectSorted(*context.scene->objectManager)) {
    if (!obj || obj->parentID != -1) {
      continue; // 子は親から再帰的に描かれる
    }
    DrawRow(context, *obj, 0);
  }
}

void OutlinerPanel::DeleteSelection(const ScenePanelContext &context) {
  ObjectSelector &selector = *context.scene->selector;
  // 反復中に選択集合が変化しないよう、ID を控えてから削除する
  const std::vector<int> ids(selector.GetSelectedIds().begin(),
                             selector.GetSelectedIds().end());
  for (const int id : ids) {
    context.scene->objectManager->DeleteObject(id);
  }
  selector.ClearSelection();
}

//=============================================================================
// パネル本体
//=============================================================================
void OutlinerPanel::Draw(const ScenePanelContext &context) {
  if (!context.IsValid() || !context.scene->selector) {
    return;
  }

  ObjectManager &objectManager = *context.scene->objectManager;
  ObjectSelector &selector = *context.scene->selector;

  // ── ヘッダ ──
  ImGui::Text("オブジェクト: %d  (選択中 %d)", objectManager.GetObjectCount(),
              static_cast<int>(selector.GetSelectedIds().size()));

  searchBox_.Draw("##outlinerSearch", "名前で絞り込み...", -1.0f);
  ImGui::Separator();

  // ── 一覧 ──
  // 高さは固定にする。-1 や「残り全部」にすると同じウィンドウに縦積みされる
  // インスペクタが画面外へ押し出されてしまう。
  constexpr float kListHeight = 240.0f;
  if (ImGui::BeginChild("##outlinerList", ImVec2(0.0f, kListHeight), true)) {
    if (searchBox_.IsEmpty()) {
      DrawHierarchy(context);
    } else {
      DrawFlatList(context);
    }
  }
  ImGui::EndChild();

  // ── フッタ ──
  ImGui::Separator();
  const bool hasSelection = selector.HasSelection();

  if (!hasSelection) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("選択を削除")) {
    DeleteSelection(context);
  }
  ImGui::SameLine();
  if (ImGui::Button("選択を複製")) {
    const std::vector<int> ids(selector.GetSelectedIds().begin(),
                               selector.GetSelectedIds().end());
    for (const int id : ids) {
      objectManager.DuplicateObject(id, {1.0f, 0.0f, 0.0f});
    }
  }
  if (!hasSelection) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();
  if (ImGui::Button("全て削除")) {
    ImGui::OpenPopup("##confirmClearAll");
  }
  if (ImGui::BeginPopup("##confirmClearAll")) {
    ImGui::TextUnformatted("シーン内の全オブジェクトを削除します。");
    if (ImGui::Button("削除する")) {
      objectManager.ClearAllObjects();
      selector.ClearSelection();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("やめる")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

} // namespace YoRigine

#endif // USE_IMGUI
