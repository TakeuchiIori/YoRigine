#ifdef USE_IMGUI

#include "SceneEditorUI.h"
#include "ObjectSelector.h"
#include "PrefabManager.h"
#include "SceneSerializer.h"

// Engine
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Collision/Core/CollisionManager.h>
#include "Object3D/ObjectManager.h"

// ImGui
#include "imgui.h"

namespace YoRigine {

    //=============================================================================
    // メニューバー拡張
    //=============================================================================
    void SceneEditorUI::DrawMenuBar() {
        if (!ImGui::BeginMenu("シーンオブジェクト")) return;

        if (ImGui::BeginMenu("ビュー")) {
            ImGui::MenuItem("オブジェクト一覧", nullptr, &showObjectList_);
            ImGui::MenuItem("トランスフォーム操作", nullptr, &showTransformControls_);

            // ---------------------------------------------------------
            // コライダーAABBデバッグ表示
            //  - 全体ON/OFF
            //  - 有効なコライダーのみ
            //  - 選択中のみ
            // ---------------------------------------------------------
            if (showColliderDebug_ != nullptr) {
                ImGui::MenuItem("コライダーAABBを表示", nullptr, showColliderDebug_);

                if (showColliderSelectedOnly_ != nullptr) {
                    ImGui::MenuItem("  選択中のみ表示", nullptr, showColliderSelectedOnly_);
                }
            }

            if (showBroadPhaseGrid_ != nullptr) {
                ImGui::Separator();
                ImGui::MenuItem("BroadPhase グリッドを表示", nullptr, showBroadPhaseGrid_);
                if (broadPhaseGridDrawRadius_ != nullptr) {
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::DragFloat("  描画半径", broadPhaseGridDrawRadius_, 1.0f, 5.0f, 500.0f, "%.1f");
                }
            }

            if (drawFrustumCulling_ != nullptr) {
                ImGui::Separator();
                ImGui::MenuItem("描画 Frustum カリングを有効化", nullptr, drawFrustumCulling_);
            }

            // コリジョン Frustum カリング (CollisionManager 直結)
            // 描画用とは別配線。ON/OFF を切り替えると BroadPhase Insert と
            // ObjectManager::Update の collider->Update() が両方スキップされる。
            {
                ImGui::Separator();
                auto* cm = YoRigine::CollisionManager::GetInstance();
                bool collisionCulling = cm->GetEnableFrustumCulling();
                if (ImGui::MenuItem("コリジョン Frustum カリングを有効化", nullptr, &collisionCulling)) {
                    cm->SetEnableFrustumCulling(collisionCulling);
                }
                // culling 統計 (直前フレーム): 視錐台外で Update をスキップした数 / 総オブジェクト数
                auto* om = ObjectManager::GetInstance();
                ImGui::Text("  culled %d / %d", om->GetLastFrameCulledCount(), om->GetLastFrameTotalCount());
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("ファイル")) {
            if (ImGui::MenuItem("配置を保存") && saveCallback_) saveCallback_();
            if (ImGui::MenuItem("配置を読み込み") && loadCallback_) loadCallback_();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("複製ツール", nullptr, &showDuplicateWindow_);
            ImGui::MenuItem("プレファブ", nullptr, &showPrefabWindow_);
            ImGui::MenuItem("コライダー設定", nullptr, &showColliderTemplates_);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }


    //=============================================================================
    // オブジェクト一覧
    //=============================================================================
    void SceneEditorUI::DrawObjectList() {
        if (!objectManager_ || !selector_) return;

        auto objects = objectManager_->GetAllActiveObjects();
        ImGui::Text("オブジェクト一覧: %d (選択中: %d)",
            static_cast<int>(objects.size()),
            static_cast<int>(selector_->GetSelectedIds().size()));
        ImGui::Separator();

        // 最新が上に来るよう逆順表示
        for (int i = static_cast<int>(objects.size()) - 1; i >= 0; --i) {
            auto* obj = objects[i];
            if (!obj) continue;

            bool isSel = selector_->IsSelected(obj->id);
            std::string label = "オブジェクト " + std::to_string(obj->id)
                + " (" + obj->modelName + ")";

            if (ImGui::Selectable(label.c_str(), isSel))
                selector_->SelectObject(obj->id, ImGui::GetIO().KeyCtrl);

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    objectManager_->DeleteObject(obj->id);
                    selector_->RemoveFromSelection(obj->id);
                }
                ImGui::EndPopup();
            }
        }

        ImGui::Separator();

        // キー入力をして選択しているオブジェクトの削除
        if (ImGui::GetIO().WantCaptureKeyboard &&
            ImGui::IsKeyPressed(ImGuiKey_Delete) &&
            selector_->HasSelection()) {
            for (int id : selector_->GetSelectedIds())
                objectManager_->DeleteObject(id);
            selector_->ClearSelection();
        }

        // UIからオブジェクトを削除
        if (selector_->HasSelection() &&
            ImGui::Button("選択されたオブジェクトを削除")) {
            for (int id : selector_->GetSelectedIds())
                objectManager_->DeleteObject(id);
            selector_->ClearSelection();
        }

        ImGui::SameLine();
        // すべて削除
        if (ImGui::Button("全て削除")) {
            objectManager_->ClearAllObjects();
            selector_->ClearSelection();
        }
    }

    //=============================================================================
    // トランスフォーム編集
    //=============================================================================
    void SceneEditorUI::DrawTransformControls() {
        if (!objectManager_ || !selector_) return;

        auto* obj = objectManager_->GetObjectById(selector_->GetPrimaryId());
        if (obj) {
            ImGui::Text("オブジェクトID %d: %s", obj->id, obj->modelName.c_str());
            ImGui::Separator();

            bool changed = false;

            if (ImGui::DragFloat3("位置", &obj->position.x, 0.1f))  changed = true;

            Vector3 rotDeg = {
                RadToDeg(obj->rotation.x),
                RadToDeg(obj->rotation.y),
                RadToDeg(obj->rotation.z)
            };
            if (ImGui::DragFloat3("回転", &rotDeg.x, 1.0f)) {
                obj->rotation = { DegToRad(rotDeg.x), DegToRad(rotDeg.y), DegToRad(rotDeg.z) };
                changed = true;
            }

            if (ImGui::DragFloat3("スケール", &obj->scale.x, 0.01f, 0.01f, 10.0f)) changed = true;

            if (changed) objectManager_->UpdateObjectTransform(*obj);

            ImGui::Separator();

            if (ImGui::Button("位置リセット")) {
                obj->position = {};
                objectManager_->UpdateObjectTransform(*obj);
            }
            ImGui::SameLine();
            if (ImGui::Button("回転リセット")) {
                obj->rotation = {};
                objectManager_->UpdateObjectTransform(*obj);
            }
            ImGui::SameLine();
            if (ImGui::Button("スケールリセット")) {
                obj->scale = { 1,1,1 };
                objectManager_->UpdateObjectTransform(*obj);
            }

            // 親子関係
            int parentId = obj->parentID;
            const char* parentLabel = (parentId >= 0)
                ? std::to_string(parentId).c_str() : "None";

            if (ImGui::BeginCombo("Parent", parentLabel)) {
                if (ImGui::Selectable("None", parentId == -1))
                    objectManager_->ClearParent(obj->id);

                for (auto* cand : objectManager_->GetAllActiveObjects()) {
                    if (!cand || cand->id == obj->id) continue;
                    if (objectManager_->HasCircularReference(obj->id, cand->id)) {
                        ImGui::BeginDisabled();
                        ImGui::Selectable(
                            ("Object " + std::to_string(cand->id) + " (循環参照)").c_str(), false);
                        ImGui::EndDisabled();
                    } else {
                        std::string lbl = "Object " + std::to_string(cand->id);
                        if (ImGui::Selectable(lbl.c_str(), cand->id == parentId))
                            objectManager_->SetParent(obj->id, cand->id);
                    }
                }
                ImGui::EndCombo();
            }

            // ── Collider セクション ────────────────────────────────────────────
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Collider")) {

                // Enable フラグ（このオブジェクト個別）
                bool enabled = obj->colliderEnabled;
                if (ImGui::Checkbox("Enable", &enabled)) {
                    obj->colliderEnabled = enabled;
                    objectManager_->ApplyColliderTemplate(*obj);
                }

                // 同名一括 ON/OFF
                ImGui::SameLine();
                if (ImGui::SmallButton("同名を全ON")) {
                    objectManager_->SetColliderEnabledAll(obj->modelName, true);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("同名を全OFF")) {
                    objectManager_->SetColliderEnabledAll(obj->modelName, false);
                }

                // ── このオブジェクト固有の設定 ──
                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                    ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                ImGui::BeginChild("##ColliderPerObj", ImVec2(0, 230), true);
                ImGui::TextDisabled("個別設定 (ID: %d)", obj->id);
                ImGui::Separator();

                // Type ドロップダウン（per-object）
                const char* currentTypeName = CollisionTypeIdToString(obj->colliderTypeId);
                if (ImGui::BeginCombo("Type", currentTypeName)) {
                    for (const auto typeId : kPlacedObjectColliderTypes) {
                        bool selected = (obj->colliderTypeId == typeId);
                        if (ImGui::Selectable(CollisionTypeIdToString(typeId), selected)) {
                            obj->colliderTypeId = typeId;
                            changed = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Shape ドロップダウン（per-object）
                const char* shapeNames[] = { "AABB", "OBB", "Sphere" };
                int currentShape = static_cast<int>(obj->colliderShapeType);
                if (ImGui::Combo("Shape", &currentShape, shapeNames, 3)) {
                    obj->colliderShapeType = static_cast<ColliderShapeType>(currentShape);
                    changed = true;
                }

                // シェイプ別オフセット
                ImGui::Separator();
                if (obj->colliderShapeType == ColliderShapeType::kAABB) {
                    ImGui::TextDisabled("AABB オフセット");
                    if (ImGui::DragFloat3("AABB Max", &obj->colliderAabbOffset.max.x, 0.05f)) changed = true;
                    if (ImGui::DragFloat3("AABB Min", &obj->colliderAabbOffset.min.x, 0.05f)) changed = true;
                } else if (obj->colliderShapeType == ColliderShapeType::kOBB) {
                    ImGui::TextDisabled("OBB オフセット");
                    if (ImGui::DragFloat3("Center##obb", &obj->colliderObbCenter.x, 0.05f)) changed = true;
                    if (ImGui::DragFloat3("Size##obb",   &obj->colliderObbSize.x,   0.05f, 0.01f, 100.0f)) changed = true;
                    if (ImGui::DragFloat3("Euler(deg)",  &obj->colliderObbEuler.x,  1.0f)) changed = true;
                } else {
                    ImGui::TextDisabled("Sphere オフセット");
                    if (ImGui::DragFloat3("Center##sph",  &obj->colliderSphereCenter.x, 0.05f)) changed = true;
                    if (ImGui::DragFloat("Radius", &obj->colliderSphereRadius, 0.05f, 0.01f, 100.0f)) changed = true;
                }

                if (changed) {
                    objectManager_->ApplyColliderTemplate(*obj);
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();

                // 明示的な一括コピーボタン
                if (ImGui::SmallButton("この設定を同名オブジェクト全てにコピー")) {
                    objectManager_->CopyColliderSettingsToAll(*obj);
                }
            }

        } else {
            ImGui::Text("オブジェクトが選択されてません");
        }

        ImGui::Separator();
        if (gizmoCtrl_) gizmoCtrl_->DrawSettings();
    }

    //=============================================================================
    // 複製ウィンドウ
    //=============================================================================
    void SceneEditorUI::DrawDuplicateWindow() {
        if (!objectManager_ || !selector_) return;

        ImGui::Text("オブジェクト複製");
        ImGui::Separator();

        auto* obj = objectManager_->GetObjectById(selector_->GetPrimaryId());
        if (obj) {
            ImGui::Text("複製対象: %s (ID: %d)", obj->modelName.c_str(), obj->id);
            ImGui::DragInt("複製数", &duplicateCount_, 1, 1, 50);
            ImGui::DragFloat3("オフセット", &duplicateOffset_.x, 0.1f);
            ImGui::Checkbox("親子関係を保持", &duplicateKeepParent_);

            if (ImGui::Button("複製実行")) {
                for (int i = 0; i < duplicateCount_; ++i) {
                    Vector3 off = duplicateOffset_ * static_cast<float>(i + 1);
                    auto* dup = objectManager_->DuplicateObject(obj->id, off);
                    if (dup && !duplicateKeepParent_)
                        objectManager_->ClearParent(dup->id);
                }
            }
        } else {
            ImGui::Text("オブジェクトを選択してください");
        }

        ImGui::Separator();
        if (ImGui::Button("閉じる")) showDuplicateWindow_ = false;
    }

    //=============================================================================
    // コライダー一覧ウィンドウ（オブジェクト個別設定）
    //=============================================================================
    void SceneEditorUI::DrawColliderTemplates() {
        if (!objectManager_) return;

        ImGui::Text("コライダー一覧（オブジェクト個別設定）");
        ImGui::Separator();

        auto objects = objectManager_->GetAllActiveObjects();
        if (objects.empty()) {
            ImGui::TextDisabled("オブジェクトがありません。");
            if (ImGui::Button("閉じる")) showColliderTemplates_ = false;
            return;
        }

        if (ImGui::BeginTable("##ColliderTable", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 0))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("ID",    ImGuiTableColumnFlags_WidthFixed,   30.0f);
            ImGui::TableSetupColumn("モデル", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthStretch, 1.5f);
            ImGui::TableSetupColumn("ON",    ImGuiTableColumnFlags_WidthFixed,   30.0f);
            ImGui::TableSetupColumn("全コピー", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (auto* obj : objects) {
                if (!obj) continue;
                ImGui::TableNextRow();
                ImGui::PushID(obj->id);

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", obj->id);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(obj->modelName.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##type", CollisionTypeIdToString(obj->colliderTypeId))) {
                    for (const auto typeId : kPlacedObjectColliderTypes) {
                        bool sel = (obj->colliderTypeId == typeId);
                        if (ImGui::Selectable(CollisionTypeIdToString(typeId), sel)) {
                            obj->colliderTypeId = typeId;
                            objectManager_->ApplyColliderTemplate(*obj);
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::TableSetColumnIndex(3);
                bool en = obj->colliderEnabled;
                if (ImGui::Checkbox("##en", &en)) {
                    obj->colliderEnabled = en;
                    objectManager_->ApplyColliderTemplate(*obj);
                }

                ImGui::TableSetColumnIndex(4);
                if (ImGui::SmallButton("同名コピー")) {
                    objectManager_->CopyColliderSettingsToAll(*obj);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        if (ImGui::Button("閉じる")) showColliderTemplates_ = false;
    }

    //=============================================================================
    // プレファブウィンドウ
    //=============================================================================
    void SceneEditorUI::DrawPrefabWindow() {
        if (!prefabMgr_ || !selector_) return;

        ImGui::Text("プレファブシステム");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("プレファブ作成")) {
            ImGui::InputText("プレファブ名", prefabNameBuf_, sizeof(prefabNameBuf_));

            if (ImGui::Button("選択オブジェクトから作成") && strlen(prefabNameBuf_) > 0) {
                prefabMgr_->CreatePrefabFromObject(
                    prefabNameBuf_, selector_->GetPrimaryId());
                memset(prefabNameBuf_, 0, sizeof(prefabNameBuf_));
            }
            ImGui::SameLine();
            if (ImGui::Button("全オブジェクトから作成") && strlen(prefabNameBuf_) > 0) {
                prefabMgr_->CreatePrefabFromAllObjects(prefabNameBuf_);
                memset(prefabNameBuf_, 0, sizeof(prefabNameBuf_));
            }
        }

        if (ImGui::CollapsingHeader("プレファブ読み込み")) {
            if (ImGui::Button("一覧を更新")) prefabMgr_->ScanPrefabFolder();

            if (ImGui::BeginListBox("##PrefabList", ImVec2(-1, 150))) {
                for (const auto& name : prefabMgr_->GetPrefabList()) {
                    if (ImGui::Selectable(name.c_str(), selectedPrefabName_ == name))
                        selectedPrefabName_ = name;
                }
                ImGui::EndListBox();
            }

            if (!selectedPrefabName_.empty()) {
                if (ImGui::Button("配置"))   prefabMgr_->LoadPrefab(selectedPrefabName_);
                ImGui::SameLine();
                if (ImGui::Button("削除")) {
                    prefabMgr_->DeletePrefab(selectedPrefabName_);
                    selectedPrefabName_.clear();
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("閉じる")) showPrefabWindow_ = false;
    }

} // namespace YoRigine

#endif // USE_IMGUI