#ifdef USE_IMGUI

#include "ColliderPanel.h"

// Engine
#include <Collision/Core/CollisionTypeIdDef.h>

namespace YoRigine {

namespace {
const char *const kShapeNames[] = {"AABB", "OBB", "Sphere"};
constexpr int kShapeCount = 3;
} // namespace

//=============================================================================
// シェイプ別のオフセット編集
//=============================================================================
void ColliderPanel::DrawShapeFields(const ScenePanelContext &context,
                                    ObjectManager::PlacedObject &obj,
                                    bool &changed) {
  (void)context;

  switch (obj.colliderShapeType) {
  case ColliderShapeType::kAABB:
    ImGui::TextDisabled("AABB オフセット");
    changed |= ImGui::DragFloat3("Max##aabb", &obj.colliderAabbOffset.max.x, 0.05f);
    changed |= ImGui::DragFloat3("Min##aabb", &obj.colliderAabbOffset.min.x, 0.05f);
    break;

  case ColliderShapeType::kOBB:
    ImGui::TextDisabled("OBB オフセット");
    changed |= ImGui::DragFloat3("中心##obb", &obj.colliderObbCenter.x, 0.05f);
    changed |= ImGui::DragFloat3("サイズ##obb", &obj.colliderObbSize.x, 0.05f, 0.01f, 100.0f);
    changed |= ImGui::DragFloat3("回転 (度)##obb", &obj.colliderObbEuler.x, 1.0f);
    break;

  case ColliderShapeType::kSphere:
  default:
    ImGui::TextDisabled("Sphere オフセット");
    changed |= ImGui::DragFloat3("中心##sph", &obj.colliderSphereCenter.x, 0.05f);
    changed |= ImGui::DragFloat("半径##sph", &obj.colliderSphereRadius, 0.05f, 0.01f, 100.0f);
    break;
  }
}

//=============================================================================
// インスペクタ内のコライダーセクション
//=============================================================================
void ColliderPanel::DrawInspectorSection(const ScenePanelContext &context,
                                         ObjectManager::PlacedObject &obj) {
  ObjectManager &objectManager = *context.scene->objectManager;
  bool changed = false;

  if (ImGui::Checkbox("有効", &obj.colliderEnabled)) {
    objectManager.ApplyColliderTemplate(obj);
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("カメラフェード", &obj.colliderCameraFade)) {
    objectManager.ApplyColliderTemplate(obj);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "カメラと重なったときにカメラを寄せず、オブジェクトを半透明にする");
  }

  // ── タイプ ──
  if (ImGui::BeginCombo("種別", CollisionTypeIdToString(obj.colliderTypeId))) {
    for (const auto typeId : kPlacedObjectColliderTypes) {
      const bool selected = (obj.colliderTypeId == typeId);
      if (ImGui::Selectable(CollisionTypeIdToString(typeId), selected)) {
        obj.colliderTypeId = typeId;
        changed = true;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // ── シェイプ ──
  int currentShape = static_cast<int>(obj.colliderShapeType);
  if (ImGui::Combo("形状", &currentShape, kShapeNames, kShapeCount)) {
    obj.colliderShapeType = static_cast<ColliderShapeType>(currentShape);
    changed = true;
  }

  ImGui::Separator();
  DrawShapeFields(context, obj, changed);

  if (changed) {
    objectManager.ApplyColliderTemplate(obj);
  }

  // ── モデル形状への自動フィット ──
  ImGui::Separator();
  ImGui::SetNextItemWidth(120.0f);
  ImGui::DragFloat("マージン", &fitMargin_, 0.01f, 1.0f, 2.0f, "x %.2f");
  ImGui::SameLine();
  if (ImGui::Button("モデルに合わせる")) {
    if (!objectManager.FitColliderToModel(obj, fitMargin_)) {
      ImGui::OpenPopup("##fitFailed");
    }
  }
  if (ImGui::BeginPopup("##fitFailed")) {
    ImGui::TextUnformatted("モデルの頂点を取得できませんでした。");
    ImGui::EndPopup();
  }

  // ── 同名モデルへの一括操作 ──
  ImGui::Separator();
  ImGui::TextDisabled("同じモデルのオブジェクトへ一括適用");
  if (ImGui::SmallButton("設定をコピー")) {
    objectManager.CopyColliderSettingsToAll(obj);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("全て有効")) {
    objectManager.SetColliderEnabledAll(obj.modelName, true);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("全て無効")) {
    objectManager.SetColliderEnabledAll(obj.modelName, false);
  }
}

//=============================================================================
// コライダー一覧ウィンドウ
//=============================================================================
void ColliderPanel::DrawOverviewWindow(const ScenePanelContext &context,
                                       bool *isOpen) {
  if (!context.IsValid()) {
    return;
  }
  ObjectManager &objectManager = *context.scene->objectManager;

  ImGui::TextUnformatted("コライダー一覧 (オブジェクト個別設定)");
  ImGui::Separator();

  auto objects = objectManager.GetAllActiveObjects();
  if (objects.empty()) {
    ImGui::TextDisabled("オブジェクトがありません。");
    if (ImGui::Button("閉じる") && isOpen) {
      *isOpen = false;
    }
    return;
  }

  if (ImGui::BeginTable("##colliderTable", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingStretchProp,
                        ImVec2(0.0f, -30.0f))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("モデル", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("種別", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("有効", ImGuiTableColumnFlags_WidthFixed, 36.0f);
    ImGui::TableSetupColumn("一括", ImGuiTableColumnFlags_WidthFixed, 76.0f);
    ImGui::TableHeadersRow();

    for (auto *obj : objects) {
      if (!obj) {
        continue;
      }
      ImGui::TableNextRow();
      ImGui::PushID(obj->id);

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d", obj->id);

      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(obj->modelName.c_str());

      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-1.0f);
      if (ImGui::BeginCombo("##type",
                            CollisionTypeIdToString(obj->colliderTypeId))) {
        for (const auto typeId : kPlacedObjectColliderTypes) {
          const bool selected = (obj->colliderTypeId == typeId);
          if (ImGui::Selectable(CollisionTypeIdToString(typeId), selected)) {
            obj->colliderTypeId = typeId;
            objectManager.ApplyColliderTemplate(*obj);
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }

      ImGui::TableSetColumnIndex(3);
      if (ImGui::Checkbox("##enabled", &obj->colliderEnabled)) {
        objectManager.ApplyColliderTemplate(*obj);
      }

      ImGui::TableSetColumnIndex(4);
      if (ImGui::SmallButton("同名コピー")) {
        objectManager.CopyColliderSettingsToAll(*obj);
      }

      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  if (ImGui::Button("閉じる") && isOpen) {
    *isOpen = false;
  }
}

} // namespace YoRigine

#endif // USE_IMGUI
