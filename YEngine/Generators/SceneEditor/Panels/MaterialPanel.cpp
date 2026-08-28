#ifdef USE_IMGUI

#include "MaterialPanel.h"

// Engine
#include "../ObjectSelector.h"
#include "Material/Material.h"
#include "Material/MaterialOverrideSet.h"
#include "Model.h"
#include <Loaders/Texture/TextureManager.h>

// C++
#include <string>

namespace YoRigine {

namespace {

// スロット index を使っているメッシュ数を数える (どのメッシュに効くか表示用)
int CountMeshesUsingSlot(const Model &model, uint32_t slotIndex) {
  int count = 0;
  for (const auto &mesh : model.GetMeshes()) {
    if (mesh->GetMaterialIndex() == slotIndex) {
      ++count;
    }
  }
  return count;
}

// スロットに実際にバインドされるテクスチャ (上書き優先, 無ければモデル本来)
std::string ResolveCurrentTexture(const MeshMaterialOverride &slot,
                                  const Material *baseMaterial) {
  if (!slot.texturePath.empty()) {
    return slot.texturePath;
  }
  return baseMaterial ? baseMaterial->GetTextureFilePath() : std::string{};
}

} // namespace

//=============================================================================
// FileBrowser の遅延生成
//=============================================================================
void MaterialPanel::EnsureBrowser() {
  if (textureBrowser_) {
    return;
  }
  // ルートは Resources/。ここから Models/ や images/ を自由に辿れるので、
  // モデルに元々付いているテクスチャ (Boss なら Resources/Models/... 配下) にも
  // 差し替え用の共通テクスチャにも 1 つのブラウザで届く。
  textureBrowser_ = std::make_unique<FileBrowser>(
      "Resources/", std::vector<std::string>{".png", ".jpg", ".jpeg", ".dds",
                                             ".tga"},
      FileBrowser::DisplayMode::Grid);

  // サムネイル生成時にテクスチャを読み込む。これにより「選んだ瞬間には
  // 既に GPU へアップロード済み」になり、差し替え直後に白くならない。
  textureBrowser_->SetThumbnailProvider(
      [](const std::string &path) -> ImTextureID {
        TextureManager::GetInstance()->LoadTexture(path);
        const auto handle =
            TextureManager::GetInstance()->GetsrvHandleGPU(path);
        return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
      });

  textureBrowser_->SetOnFileSelected([this](const std::string &path) {
    pickedTexturePath_ = path;
    showTextureBrowser_ = false;
  });
}

//=============================================================================
// モデル本来の値の表示
//=============================================================================
void MaterialPanel::DrawSourceInfo(const ObjectManager::PlacedObject &obj) {
  Model *model = obj.object ? obj.object->GetModel() : nullptr;
  if (!model) {
    return;
  }

  ImGui::TextDisabled("モデル: %s   メッシュ %d / マテリアル %d",
                      obj.modelName.c_str(),
                      static_cast<int>(model->GetMeshes().size()),
                      static_cast<int>(model->GetMaterialCount()));
}

//=============================================================================
// 1 スロット
//=============================================================================
void MaterialPanel::DrawSlot(const ScenePanelContext &context,
                             ObjectManager::PlacedObject &obj,
                             size_t slotIndex) {
  Model *model = obj.object->GetModel();
  Material *baseMaterial = model->GetMaterial(slotIndex);

  const std::string slotName =
      baseMaterial && !baseMaterial->GetName().empty()
          ? baseMaterial->GetName()
          : ("スロット " + std::to_string(slotIndex));
  const int meshCount =
      CountMeshesUsingSlot(*model, static_cast<uint32_t>(slotIndex));

  const std::string header =
      slotName + "  (メッシュ " + std::to_string(meshCount) + " 枚)";

  if (!ImGui::CollapsingHeader(header.c_str(),
                               slotIndex == 0 ? ImGuiTreeNodeFlags_DefaultOpen
                                              : 0)) {
    return;
  }

  MaterialOverrideSet *overrides =
      context.scene->objectManager->GetOrCreateMaterialOverrides(obj);
  if (!overrides) {
    return;
  }
  MeshMaterialOverride *slot = overrides->GetSlot(slotIndex);
  if (!slot) {
    return;
  }

  const std::string id = "slot" + std::to_string(slotIndex);
  const std::string currentTexture = ResolveCurrentTexture(*slot, baseMaterial);

  const YEditorWidget::MaterialSlotResult result =
      slotEditor_.Draw(id.c_str(), *slot, baseMaterial, currentTexture);

  if (result.changed) {
    overrides->MarkDirty();
  }
  // テクスチャ選択ブラウザを開く要求 → このスロットを対象にして開く
  if (result.requestOpenTexture) {
    EnsureBrowser();
    browserTargetSlot_ = static_cast<int>(slotIndex);
    showTextureBrowser_ = true;
    textureBrowser_->Scan();
  }
  // テクスチャ上書きを解除 (モデル本来のテクスチャへ戻す)
  if (result.clearTexture) {
    overrides->SetSlotTexture(slotIndex, "");
    overrides->MarkDirty();
  }

  if (slot->IsActive()) {
    if (ImGui::SmallButton("このスロットの上書きを解除")) {
      *slot = MeshMaterialOverride{};
      overrides->MarkDirty();
    }
  }
}

//=============================================================================
// テクスチャ選択ブラウザ (モーダル)
//=============================================================================
void MaterialPanel::DrawTextureBrowser(const ScenePanelContext &context,
                                       ObjectManager::PlacedObject &obj) {
  if (!textureBrowser_) {
    return;
  }

  if (showTextureBrowser_) {
    ImGui::OpenPopup("##materialTexBrowser");
  }

  ImGui::SetNextWindowSize(ImVec2(560, 460), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##materialTexBrowser", &showTextureBrowser_,
                             ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize)) {
    ImGui::Text("テクスチャを選択 (スロット %d)", browserTargetSlot_);
    ImGui::Separator();
    textureBrowser_->Draw("##materialTexChild", ImVec2(0, 360));
    ImGui::Separator();
    if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
      showTextureBrowser_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // 選択が確定していれば対象スロットへ反映する。
  // SetSlotTexture が TextureManager::LoadTexture まで面倒を見る
  // (サムネイルで既にロード済みなので実質ノーコストで引ける)。
  if (pickedTexturePath_.empty() || browserTargetSlot_ < 0) {
    return;
  }
  MaterialOverrideSet *overrides =
      context.scene->objectManager->GetOrCreateMaterialOverrides(obj);
  if (overrides) {
    overrides->SetSlotTexture(static_cast<size_t>(browserTargetSlot_),
                              pickedTexturePath_);
    overrides->MarkDirty();
  }
  pickedTexturePath_.clear();
  browserTargetSlot_ = -1;
}

//=============================================================================
// パネル本体
//=============================================================================
void MaterialPanel::Draw(const ScenePanelContext &context) {
  if (!context.IsValid() || !context.scene->selector) {
    return;
  }

  auto *obj = context.scene->objectManager->GetObjectById(
      context.scene->selector->GetPrimaryId());
  if (!obj || !obj->object) {
    ImGui::TextDisabled("オブジェクトを選択してください");
    return;
  }

  Model *model = obj->object->GetModel();
  if (!model) {
    ImGui::TextDisabled("モデルが読み込まれていません");
    return;
  }

  DrawSourceInfo(*obj);
  ImGui::Separator();

  const size_t slotCount = model->GetMaterialCount();
  if (slotCount == 0) {
    ImGui::TextDisabled("マテリアルがありません");
    return;
  }

  for (size_t i = 0; i < slotCount; ++i) {
    ImGui::PushID(static_cast<int>(i));
    DrawSlot(context, *obj, i);
    ImGui::PopID();
  }

  // テクスチャ選択ブラウザ (開いていれば) と選択結果の反映
  DrawTextureBrowser(context, *obj);

  ImGui::Separator();
  MaterialOverrideSet *overrides =
      context.scene->objectManager->GetMaterialOverrides(*obj);
  if (overrides && overrides->HasAnyOverride()) {
    if (ImGui::Button("すべての上書きを解除")) {
      overrides->ClearAll();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("解除するとモデル本来の値 (Blender の設定) に戻る");
  } else {
    ImGui::TextDisabled("上書きなし: Blender で設定した値で描画されている");
  }
}

} // namespace YoRigine

#endif // USE_IMGUI
