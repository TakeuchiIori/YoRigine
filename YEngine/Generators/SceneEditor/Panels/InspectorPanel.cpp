#ifdef USE_IMGUI

#include "InspectorPanel.h"

// Engine
#include "../Edit/SceneGizmoLayer.h"
#include "../Edit/ScenePlacementService.h"
#include "../ObjectSelector.h"
#include "Model.h"
#include <Motion/Editor/MotionEditor.h>

// C++
#include <cstdio>

namespace YoRigine {

//=============================================================================
// ヘッダ (名前 / モデル)
//=============================================================================
void InspectorPanel::DrawHeader(ObjectManager::PlacedObject &obj) {
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s", obj.nameTag.c_str());

  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputTextWithHint("##nameTag", "名前 (未命名)", buffer,
                               sizeof(buffer))) {
    obj.nameTag = buffer;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("シーン内で一意な名前。TriggerAction "
                      "などがターゲットを参照するのに使う");
  }

  ImGui::TextDisabled("ID %d   モデル: %s", obj.id, obj.modelName.c_str());
  ImGui::Separator();
}

//=============================================================================
// モーションエディタのトグル
//=============================================================================
void InspectorPanel::DrawMotionEditorToggle(const ScenePanelContext &context,
                                            ObjectManager::PlacedObject &obj) {
  MotionEditor *motionEditor = context.scene->motionEditor;
  if (!motionEditor) {
    return;
  }

  // モーションを持たないモデルはエディタ側で対象外になるので出さない
  Model *model = obj.object ? obj.object->GetModel() : nullptr;
  if (!model || !model->GetMotionSystem()) {
    return;
  }

  bool enabled = motionEditor->IsEnabled();
  if (ImGui::Checkbox("モーションエディタで編集", &enabled)) {
    motionEditor->SetEnabled(enabled);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("ON の間だけ Motion Editor "
                      "が開き、選択中のモデルを編集対象にする");
  }
  ImGui::Separator();
}

//=============================================================================
// トランスフォームタブ
//=============================================================================
void InspectorPanel::DrawTransformTab(const ScenePanelContext &context,
                                      ObjectManager::PlacedObject &obj) {
  ObjectManager &objectManager = *context.scene->objectManager;
  bool changed = false;

  changed |= transformFields_.Draw(obj.position, obj.rotation, obj.scale);

  ImGui::Separator();
  changed |= transformFields_.DrawResetButtons(obj.position, obj.rotation, obj.scale);

  // ── アンカーポイント (回転の旋回中心) ──
  ImGui::Separator();
  changed |= ImGui::Checkbox("アンカーを使う", &obj.useAnchorPoint);
  if (obj.useAnchorPoint) {
    changed |=
        ImGui::DragFloat3("アンカー位置 (ローカル)", &obj.anchorPoint.x, 0.05f);
    ImGui::TextDisabled("ヒンジ扉なら端の位置を入れる (例: 0.5, 0, 0)");
  }

  if (changed) {
    objectManager.UpdateObjectTransform(obj);
  }

  // ── 配置補助 ──
  ImGui::Separator();
  ImGui::TextDisabled("配置補助");
  if (ImGui::Button("地面に吸着 (Ctrl+G)") && context.placement) {
    context.placement->SnapSelectionToSurface();
  }
  ImGui::SameLine();
  const bool stamping = context.isStamping && context.isStamping();
  if (stamping) {
    if (ImGui::Button("スタンプ配置を終了 (Esc)") && context.exitStamp) {
      context.exitStamp();
    }
  } else {
    if (ImGui::Button("スタンプ配置 (B)") && context.startStamp) {
      context.startStamp();
    }
  }

  // ── ギズモ設定 ──
  if (context.gizmoLayer) {
    ImGui::Separator();
    context.gizmoLayer->GetController().DrawSettings();
  }
}

//=============================================================================
// レンダリングタブ
//=============================================================================
void InspectorPanel::DrawRenderingTab(const ScenePanelContext &context,
                                      ObjectManager::PlacedObject &obj) {
  ObjectManager &objectManager = *context.scene->objectManager;

  // ── 乗算カラー (マテリアルとは別に、オブジェクト単位で色を掛ける) ──
  if (ImGui::ColorEdit4("乗算カラー", &obj.color.x,
                        ImGuiColorEditFlags_AlphaBar |
                            ImGuiColorEditFlags_AlphaPreviewHalf)) {
    objectManager.ApplyObjectColor(obj);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("白に戻す")) {
    obj.color = {1.0f, 1.0f, 1.0f, 1.0f};
    objectManager.ApplyObjectColor(obj);
  }

  // ── UV ──
  ImGui::Separator();
  if (ImGui::DragFloat2("UV スケール", &obj.uvScale.x, 0.01f, 0.0f, 100.0f,
                        "%.3f")) {
    objectManager.ApplyObjectUV(obj);
  }
  if (ImGui::SliderFloat("UV ランダム化", &obj.uvStochastic, 0.0f, 1.0f,
                         "%.2f")) {
    objectManager.ApplyObjectUV(obj);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("タイル単位でハッシュ回転・オフセットを掛け、"
                      "タイリングの繰り返し感を消す");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("UV リセット")) {
    obj.uvScale = {1.0f, 1.0f};
    obj.uvStochastic = 0.0f;
    objectManager.ApplyObjectUV(obj);
  }

  // ── 描画フラグ ──
  ImGui::Separator();
  ImGui::Checkbox("ビューポートに表示", &obj.visible);
  ImGui::Checkbox("アウトライン", &obj.outlineEnabled);
  ImGui::Checkbox("影を落とす", &obj.castShadow);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("巨大な地面はシャドウマップを埋めて影がチラつくので OFF "
                      "にするとよい");
  }
  ImGui::Checkbox("クリックで選択できる", &obj.pickable);
}

//=============================================================================
// 階層タブ
//=============================================================================
void InspectorPanel::DrawHierarchyTab(const ScenePanelContext &context,
                                      ObjectManager::PlacedObject &obj) {
  ObjectManager &objectManager = *context.scene->objectManager;

  const std::string parentLabel =
      obj.parentID >= 0 ? ("オブジェクト " + std::to_string(obj.parentID))
                        : std::string("なし");

  if (ImGui::BeginCombo("親", parentLabel.c_str())) {
    if (ImGui::Selectable("なし", obj.parentID == -1)) {
      objectManager.ClearParent(obj.id);
    }

    for (auto *candidate : objectManager.GetAllActiveObjects()) {
      if (!candidate || candidate->id == obj.id) {
        continue;
      }
      const std::string label =
          "オブジェクト " + std::to_string(candidate->id) + " (" +
          candidate->modelName + ")";

      if (objectManager.HasCircularReference(obj.id, candidate->id)) {
        ImGui::BeginDisabled();
        ImGui::Selectable((label + "  ※循環参照").c_str(), false);
        ImGui::EndDisabled();
        continue;
      }
      if (ImGui::Selectable(label.c_str(), candidate->id == obj.parentID)) {
        objectManager.SetParent(obj.id, candidate->id);
      }
    }
    ImGui::EndCombo();
  }

  const auto children = objectManager.GetChildObjects(obj.id);
  ImGui::Separator();
  ImGui::Text("子オブジェクト: %d", static_cast<int>(children.size()));
  for (auto *child : children) {
    if (child) {
      ImGui::BulletText("%d : %s", child->id, child->modelName.c_str());
    }
  }
}

//=============================================================================
// パネル本体
//=============================================================================
void InspectorPanel::Draw(const ScenePanelContext &context) {
  if (!context.IsValid() || !context.scene->selector) {
    return;
  }

  auto *obj = context.scene->objectManager->GetObjectById(
      context.scene->selector->GetPrimaryId());
  if (!obj) {
    ImGui::TextDisabled("オブジェクトが選択されていません");
    return;
  }

  DrawHeader(*obj);
  DrawMotionEditorToggle(context, *obj);

  if (!ImGui::BeginTabBar("##inspectorTabs")) {
    return;
  }

  if (ImGui::BeginTabItem("トランスフォーム")) {
    DrawTransformTab(context, *obj);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("描画")) {
    DrawRenderingTab(context, *obj);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("マテリアル")) {
    materialPanel_.Draw(context);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("コライダー")) {
    colliderPanel_.DrawInspectorSection(context, *obj);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("階層")) {
    DrawHierarchyTab(context, *obj);
    ImGui::EndTabItem();
  }

  ImGui::EndTabBar();
}

} // namespace YoRigine

#endif // USE_IMGUI
