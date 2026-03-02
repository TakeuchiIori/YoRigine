#include "ModelManipulator.h"

#include <filesystem>
#include <algorithm>
#include <iostream>

#include "ModelManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"
#include <Editor/Editor.h>
#include <Debugger/Gizmo/IGizmable.h>
#include <DirectX/DirectXCommon.h>
#endif

namespace YoRigine {

    ModelManipulator* ModelManipulator::instance_ = nullptr;

    ModelManipulator* ModelManipulator::GetInstance()
    {
        if (!instance_) instance_ = new ModelManipulator;
        return instance_;
    }

    void ModelManipulator::Initialize()
    {
        if (isInitialized_) return; // 重複初期化を防止

        objectManager_ = ObjectManager::GetInstance();
        serializer_.SetObjectManager(objectManager_);
        serializer_.SetModelFolderPath(modelFolderPath_);

        prefabMgr_.SetObjectManager(objectManager_);
        prefabMgr_.SetSerializer(&serializer_);
        prefabMgr_.ScanPrefabFolder();

        selector_.SetObjectManager(objectManager_);

#ifdef USE_IMGUI
        pickBuffer_ = PickBuffer::GetInstance();
        pickBuffer_->Initialize(); // PickBuffer もここで一度だけ初期化
        selector_.SetPickBuffer(pickBuffer_);

        browser_.SetModelFolderPath(modelFolderPath_);
        browser_.SetPlaceCallback([this](const std::string& path) { PlaceObject(path); });
        browser_.ScanModelFolder();

        editorUI_.SetObjectManager(objectManager_);
        editorUI_.SetSelector(&selector_);
        editorUI_.SetPrefabManager(&prefabMgr_);
        editorUI_.SetSerializer(&serializer_);
        editorUI_.SetGizmoController(&gizmoCtrl_);
        editorUI_.SetPlaceCallback([this](const std::string& path) { PlaceObject(path); });
        editorUI_.SetSaveCallback([this]() { serializer_.SaveScene(jsonPath_); });
        editorUI_.SetLoadCallback([this]() { serializer_.LoadScene(jsonPath_); });

        gizmoCtrl_.Initialize();

        // Editor へのメニュー登録もここで一度だけ行う
        Editor::GetInstance()->RegisterMenuBar([this] { editorUI_.DrawMenuBar(); });
#endif

        isInitialized_ = true;
    }

    // =============================================================================
    // Update
    //
    // ■ Pick Pass の順番
    //   1. BeginPickPass()     ← RT クリア・パイプラインセット
    //   2. DrawForPick()       ← 全オブジェクトを PickPSO で描画（IDを焼き込む）
    //   3. EndPickPass()       ← クリックがあった座標を 1px コピー + Signal
    //   4. selector_.Update()  ← クリック検出時は RequestPick、
    //                            前フレームの結果があれば ReadPickResult
    //
    //   PickBuffer の RT にはこのフレームの描画結果が入っている。
    //   クリック座標は RequestPick で登録され、EndPickPass でコピーされ、
    //   次フレームの Update の先頭で ReadPickResult が読み取る。
    // =============================================================================
    void ModelManipulator::Update()
    {
        if (!isInitialized_) return;

#ifdef USE_IMGUI
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
                serializer_.SaveScene(jsonPath_);
                std::cout << "[ModelManipulator] Saved (Ctrl+S)\n";
            }
        }

        ShortcutKey();

        // ── selector_.Update() だけここで行う（GPU命令は積まない）──
        selector_.SetCamera(camera_);
        selector_.Update(
            true,
            Editor::GetInstance()->GetGameViewPos(),
            Editor::GetInstance()->GetGameViewSize());
#endif

        objectManager_->Update();
    }
    void ModelManipulator::Draw()
    {
        if (!isInitialized_ || !camera_) return;
        for (auto* obj : objectManager_->GetAllActiveObjects()) {
            if (obj && obj->object && obj->worldTransform)
                obj->object->Draw(camera_, *obj->worldTransform);
        }
    }

    void ModelManipulator::DrawPickPass()
    {
#ifdef USE_IMGUI
        if (!isInitialized_ || !pickBuffer_) return;

        pickBuffer_->BeginPickPass();
        DrawForPick();
        pickBuffer_->EndPickPass();
#endif
    }

    void ModelManipulator::DrawShadow()
    {
        if (!isInitialized_) return;
        for (auto* obj : objectManager_->GetAllActiveObjects()) {
            if (obj && obj->object && obj->worldTransform)
                obj->object->DrawShadow(*obj->worldTransform);
        }
    }

    void ModelManipulator::DrawImGui()
    {
#ifdef USE_IMGUI
        if (!isInitialized_) return;

        if (!ImGui::GetIO().WantCaptureKeyboard &&
            ImGui::IsKeyPressed(ImGuiKey_Delete) &&
            selector_.HasSelection()) {
            for (int id : selector_.GetSelectedIds())
                objectManager_->DeleteObject(id);
            selector_.ClearSelection();
        }

        browser_.Draw();
        ImGui::Separator();

        if (editorUI_.GetShowObjectListPtr() && *editorUI_.GetShowObjectListPtr())
            editorUI_.DrawObjectList();
        if (editorUI_.GetShowTransformControlsPtr() && *editorUI_.GetShowTransformControlsPtr())
            editorUI_.DrawTransformControls();
        if (*editorUI_.GetShowDuplicateWindowPtr())
            editorUI_.DrawDuplicateWindow();
        if (*editorUI_.GetShowPrefabWindowPtr())
            editorUI_.DrawPrefabWindow();
#endif
    }

    void ModelManipulator::DrawGizmo()
    {
#ifdef USE_IMGUI
        if (!camera_ || !selector_.HasSelection()) return;

        gizmables_.clear();
        std::vector<IGizmable*> targets;

        // 複数選択時でもギズモは primaryId_（最後に選択したオブジェクト）の1つだけ表示する。
        // 全選択IDにギズモを出すと ImGuizmo が複数重なって操作できなくなるため。
        auto* obj = objectManager_->GetObjectById(selector_.GetPrimaryId());
        if (obj && obj->worldTransform) {
            gizmables_.emplace_back(obj, objectManager_);
            targets.push_back(&gizmables_.back());
        }

        if (targets.empty()) return;
        if (!selector_.HasSelection()) return;
        gizmoCtrl_.Draw(
            camera_,
            targets,
            Editor::GetInstance()->GetGameViewPos(),
            Editor::GetInstance()->GetGameViewSize());
#endif
    }

    // =============================================================================
    // DrawForPick
    //   BeginPickPass / EndPickPass の間に呼ぶ。
    //   DrawShadow と同じ頂点/インデックスバッファを流用し、
    //   PickPSO で ObjectID を R32_UINT RT に書き込む。
    // =============================================================================
    void ModelManipulator::DrawForPick()
    {
#ifdef USE_IMGUI
        if (!camera_ || !pickBuffer_) return;

        auto* cmd = DirectXCommon::GetInstance()->GetCommandList().Get();

        for (auto* obj : objectManager_->GetAllActiveObjects()) {
            if (!obj || !obj->object || !obj->worldTransform) continue;

            auto* model = obj->object->GetModel();
            if (!model) continue;

            // WVP 行列 CBV (b0)
            cmd->SetGraphicsRootConstantBufferView(
                PickBuffer::ROOT_PARAM_MVP_CBV,
                obj->worldTransform->GetConstBuffer()->GetGPUVirtualAddress());

            // ObjectID ルート定数 (b1)  ※ 0 は空選択予約なので +1
            uint32_t encodedID = static_cast<uint32_t>(obj->id) + 1;
            cmd->SetGraphicsRoot32BitConstant(
                PickBuffer::ROOT_PARAM_OBJECT_ID, encodedID, 0);

            // 頂点/インデックス描画（DrawShadow と同じ方式）
            for (auto& mesh : model->GetMeshes()) {
                if (mesh->HasBones() && model->GetSkinCluster()) {
                    mesh->RecordDrawCommands(cmd, *model->GetSkinCluster());
                }
                else {
                    mesh->RecordDrawCommands(cmd);
                }
                cmd->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
            }
        }
#endif
    }

    void ModelManipulator::Finalize()
    {
#ifdef USE_IMGUI
        // 状態リセットのみ（GPU同期は不要）
        if (pickBuffer_) {
            pickBuffer_->Reset();
        }
#endif

        if (objectManager_) objectManager_->Finalize();
        delete instance_;
        instance_ = nullptr;
    }

    void ModelManipulator::LoadScene(const std::string& sceneName) {
        // システムが初期化されていなければ初期化する (安全策)
        if (!isInitialized_) Initialize();

        // *** GPU��期はSceneManagerが行うため削除 ***

#ifdef USE_IMGUI
    // PickBufferの状態をリセット
        if (pickBuffer_) {
            pickBuffer_->Reset();
        }
#endif

        jsonPath_ = "Resources/Json/Scenes/" + sceneName + ".json";

        // 1. 前のシーンのオブジェクトを安全にクリア
        objectManager_->ClearAllObjects();

        // 2. 新しいシーンデータをロード
        serializer_.LoadScene(jsonPath_);

        // 3. 選択状態をリセット
        selector_.ClearSelection();

        std::cout << "[ModelManipulator] Scene Loaded: " << sceneName << "\n";
    }

    void ModelManipulator::SetCamera(Camera* camera)
    {
        camera_ = camera;
        selector_.SetCamera(camera);
    }

    /// <summary>
    /// ショートカットキーの処理
    /// </summary>
    void ModelManipulator::ShortcutKey()
    {
#ifdef USE_IMGUI
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_C)) {
                CopyObject();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_V)) {
                PasteObject();
            }
        }
#endif
    }

    void ModelManipulator::PlaceObject(const std::string& modelPath)
    {
        try {
            if (modelPath.empty() || !std::filesystem::exists(modelPath)) {
                std::cout << "[ModelManipulator] Invalid path: " << modelPath << "\n";
                return;
            }

            std::filesystem::path full(modelPath);
            std::filesystem::path rel = std::filesystem::relative(full, modelFolderPath_);

            std::string ext = full.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            bool isAnim = (ext == ".gltf" || ext == ".glb");

            auto* obj = objectManager_->CreateObject(rel.string(), isAnim);
            if (obj) {
                selector_.ClearSelection();
                selector_.AddToSelection(obj->id);
                objectManager_->UpdateObjectTransform(*obj);
                std::cout << "[ModelManipulator] Placed: " << full.filename() << "\n";
            }
        }
        catch (const std::exception& e) {
            std::cout << "[ModelManipulator] PlaceObject error: " << e.what() << "\n";
        }
    }

    /// <summary>
    /// 選択したオブジェクトのコピー
    /// </summary>
    void ModelManipulator::CopyObject()
    {
        auto& selectID = selector_.GetSelectedIds();
        if (selectID.empty())return;

		copyObjectIDs_.assign(selectID.begin(), selectID.end());
    }

    /// <summary>
    /// コピーしたオブジェクトを貼り付け
    /// </summary>
    void ModelManipulator::PasteObject()
    {
        if (copyObjectIDs_.empty()) return;
        // 貼り付けたものを新しく選択状態にするためにクリアする
        selector_.ClearSelection();

        for (int id : copyObjectIDs_) {
            auto* srcObj = objectManager_->GetObjectById(id);
            if (!srcObj) continue;

            // 生成元のオブジェクト情報を参照してコピーを作成
            auto* newObj = objectManager_->CreateObject(srcObj->modelName, srcObj->isAnimation);
            newObj->position = { srcObj->position + offsetCopyPos_ };
			newObj->rotation = srcObj->rotation;
			newObj->scale = srcObj->scale;

            // 選択状態に追加
			selector_.AddToSelection(newObj->id);
			objectManager_->UpdateObjectTransform(*newObj);
        }
    }
} // namespace YoRigine