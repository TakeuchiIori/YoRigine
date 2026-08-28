#ifdef USE_IMGUI

#include "PrefabPanel.h"

// Engine
#include "../ObjectSelector.h"
#include "../PrefabManager.h"

// C++
#include <cstring>

namespace YoRigine {

void PrefabPanel::DrawCreateSection(const ScenePanelContext &context) {
  PrefabManager &prefabManager = *context.scene->prefabManager;
  ObjectSelector &selector = *context.scene->selector;

  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##prefabName", "プレファブ名", newPrefabName_,
                           sizeof(newPrefabName_));

  const bool hasName = (std::strlen(newPrefabName_) > 0);
  if (!hasName) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("選択オブジェクトから作成")) {
    prefabManager.CreatePrefabFromObject(newPrefabName_,
                                         selector.GetPrimaryId());
    std::memset(newPrefabName_, 0, sizeof(newPrefabName_));
  }
  ImGui::SameLine();
  if (ImGui::Button("シーン全体から作成")) {
    prefabManager.CreatePrefabFromAllObjects(newPrefabName_);
    std::memset(newPrefabName_, 0, sizeof(newPrefabName_));
  }

  if (!hasName) {
    ImGui::EndDisabled();
    ImGui::TextDisabled("名前を入力すると作成できる");
  }
}

void PrefabPanel::DrawLibrarySection(const ScenePanelContext &context) {
  PrefabManager &prefabManager = *context.scene->prefabManager;

  if (ImGui::Button("一覧を更新")) {
    prefabManager.ScanPrefabFolder();
  }
  ImGui::SameLine();
  searchBox_.Draw("##prefabSearch", "絞り込み...", 180.0f);

  if (ImGui::BeginListBox("##prefabList", ImVec2(-1.0f, 180.0f))) {
    for (const auto &name : prefabManager.GetPrefabList()) {
      if (!searchBox_.Matches(name)) {
        continue;
      }
      if (ImGui::Selectable(name.c_str(), selectedPrefabName_ == name)) {
        selectedPrefabName_ = name;
      }
    }
    ImGui::EndListBox();
  }

  if (selectedPrefabName_.empty()) {
    ImGui::TextDisabled("プレファブを選ぶと配置・削除できる");
    return;
  }

  if (ImGui::Button("シーンに配置")) {
    prefabManager.LoadPrefab(selectedPrefabName_);
  }
  ImGui::SameLine();
  if (ImGui::Button("削除")) {
    ImGui::OpenPopup("##confirmPrefabDelete");
  }
  if (ImGui::BeginPopup("##confirmPrefabDelete")) {
    ImGui::Text("%s を削除します。", selectedPrefabName_.c_str());
    if (ImGui::Button("削除する")) {
      prefabManager.DeletePrefab(selectedPrefabName_);
      selectedPrefabName_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("やめる")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void PrefabPanel::Draw(const ScenePanelContext &context, bool *isOpen) {
  if (!context.IsValid() || !context.scene->prefabManager ||
      !context.scene->selector) {
    return;
  }

  ImGui::TextUnformatted("プレファブ");
  ImGui::Separator();

  if (ImGui::CollapsingHeader("作成", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawCreateSection(context);
  }
  if (ImGui::CollapsingHeader("ライブラリ", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawLibrarySection(context);
  }

  ImGui::Separator();
  if (ImGui::Button("閉じる") && isOpen) {
    *isOpen = false;
  }
}

} // namespace YoRigine

#endif // USE_IMGUI
