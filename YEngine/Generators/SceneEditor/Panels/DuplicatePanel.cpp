#ifdef USE_IMGUI

#include "DuplicatePanel.h"

// Engine
#include "../ObjectSelector.h"

// C++
#include <vector>

namespace YoRigine {

void DuplicatePanel::Execute(const ScenePanelContext &context) {
  ObjectManager &objectManager = *context.scene->objectManager;
  ObjectSelector &selector = *context.scene->selector;

  // 複製で選択集合が変化するため、元の ID を控えてから回す
  std::vector<int> sourceIds;
  if (applyToAllSelected_) {
    sourceIds.assign(selector.GetSelectedIds().begin(),
                     selector.GetSelectedIds().end());
  } else {
    sourceIds.push_back(selector.GetPrimaryId());
  }

  for (const int sourceId : sourceIds) {
    for (int i = 0; i < count_; ++i) {
      const Vector3 delta = offset_ * static_cast<float>(i + 1);
      auto *duplicate = objectManager.DuplicateObject(sourceId, delta);
      if (duplicate && !keepParent_) {
        objectManager.ClearParent(duplicate->id);
      }
    }
  }
}

void DuplicatePanel::Draw(const ScenePanelContext &context, bool *isOpen) {
  if (!context.IsValid() || !context.scene->selector) {
    return;
  }

  ImGui::TextUnformatted("オブジェクト複製");
  ImGui::Separator();

  auto *obj = context.scene->objectManager->GetObjectById(
      context.scene->selector->GetPrimaryId());
  if (!obj) {
    ImGui::TextDisabled("オブジェクトを選択してください");
    if (ImGui::Button("閉じる") && isOpen) {
      *isOpen = false;
    }
    return;
  }

  ImGui::Text("複製対象: %s (ID %d)", obj->modelName.c_str(), obj->id);
  ImGui::Separator();

  ImGui::DragInt("個数", &count_, 1, 1, 50);
  ImGui::DragFloat3("1 個あたりのオフセット", &offset_.x, 0.1f);
  ImGui::Checkbox("親子関係を保持", &keepParent_);
  ImGui::Checkbox("選択中すべてを複製", &applyToAllSelected_);

  ImGui::Separator();
  if (ImGui::Button("複製する")) {
    Execute(context);
  }
  ImGui::SameLine();
  if (ImGui::Button("閉じる") && isOpen) {
    *isOpen = false;
  }
}

} // namespace YoRigine

#endif // USE_IMGUI
