#pragma once

// C++
#include <string>
#include <vector>

// Engine
#include <Object3D/ObjectManager.h>
#include <Systems/Camera/Camera.h>
#include <Motion/Editor/MotionEditor.h>

#ifdef USE_IMGUI
#include "ModelBrowser.h"
#include "SceneEditorUI.h"
#include <Debugger/Gizmo/GizmoController.h>
#include "PlacedObjectGizmable.h"
#endif

// Subsystems
#include "SceneSerializer.h"
#include <Graphics/Drawer/LineManager/Line.h>
#include "PrefabManager.h"
#include "ObjectSelector.h"
#include "PickBuffer.h"   

namespace YoRigine {

    class ModelManipulator
    {
    public:
        //=========================================================================
        // 基本
        //=========================================================================
        static ModelManipulator* GetInstance();

        void Initialize();
        void Update();
        void Draw();
        void DrawLine();
        void DrawPickPass();
        void DrawShadow();
        void DrawImGui();
        void DrawGizmo();
        void DrawForPick();
        void Finalize();

        void PlaceObject(const std::string& modelPath);
        void LoadScene(const std::string& sceneName);

        void SetCamera(Camera* camera) {
            camera_ = camera;
            selector_.SetCamera(camera);
            motionEditor_.SetCamera(camera);
            colliderLineStaticWall_.SetCamera(camera_);
            colliderLineNavObstacle_.SetCamera(camera_);
            colliderLineNavTrigger_.SetCamera(camera_);
            colliderLineWaypoint_.SetCamera(camera_);
            colliderLineDefault_.SetCamera(camera_);
            objectManager_->SetCamera(camera);
        }

        //=========================================================================
        // サブシステムアクセッサ
        //=========================================================================
        SceneSerializer& GetSerializer() { return serializer_; }
        ObjectSelector& GetSelector() { return selector_; }
        MotionEditor& GetMotionEditor() { return motionEditor_; }

#ifdef USE_IMGUI
        //=========================================================================
        // デバッグ描画設定（SceneEditorUI から操作）
        //=========================================================================
        bool* GetShowColliderDebugPtr() { return &showColliderDebug_; }
        bool* GetShowColliderSelectedOnlyPtr() { return &showColliderSelectedOnly_; }
#endif

    private:
        //=========================================================================
        // 内部処理
        //=========================================================================
        void ShortcutKey();
        void CopyObject();
        void PasteObject();

        //=========================================================================
        // シングルトン
        //=========================================================================
        ModelManipulator() = default;
        ~ModelManipulator() = default;
        ModelManipulator(const ModelManipulator&) = delete;
        ModelManipulator& operator=(const ModelManipulator&) = delete;
        ModelManipulator(ModelManipulator&&) = delete;
        ModelManipulator& operator=(ModelManipulator&&) = delete;

        static ModelManipulator* instance_;

        //=========================================================================
        // メンバ変数
        //=========================================================================
        Camera* camera_ = nullptr;
        ObjectManager* objectManager_ = nullptr;
        PickBuffer* pickBuffer_ = nullptr;
        bool           isInitialized_ = false;
        std::string    jsonPath_;
        std::string    modelFolderPath_ = "Resources/Models/";
        MotionEditor motionEditor_;

        // タイプごとに独立したLineインスタンスを持つ
        // (単一バッファを複数Draw間で共有するとGPU実行時にCPU上書きが起きるため)
        Line   colliderLineStaticWall_;   // 赤
        Line   colliderLineNavObstacle_;  // 黄
        Line   colliderLineNavTrigger_;   // 青
        Line   colliderLineWaypoint_;     // 緑
        Line   colliderLineDefault_;      // グレー
        bool   showColliderDebug_ = true; // コライダー表示フラグ

#ifdef USE_IMGUI
        bool   showColliderSelectedOnly_ = false; // 選択中のみ描画
#endif

        std::vector<int> copyObjectIDs_;
        Vector3 offsetCopyPos_ = { 1.0f,0.0f,0.0f };

        SceneSerializer serializer_;
        PrefabManager   prefabMgr_;
        ObjectSelector  selector_;

#ifdef USE_IMGUI
        ModelBrowser  browser_;
        SceneEditorUI editorUI_;
        GizmoController gizmoCtrl_;
        std::vector<PlacedObjectGizmable> gizmables_;
#endif
    };

} // namespace YoRigine