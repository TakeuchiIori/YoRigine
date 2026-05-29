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

    /// <summary>
    /// シーンエディター統括クラス。
    /// 各サブシステムを保持し、外部には
    ///   Initialize / Update / Draw / DrawShadow / DrawImGui / DrawGizmo / Finalize
    /// の7本のみを公開する。
    ///
    ///  サブシステム構成
    ///   SceneSerializer … JSON Save/Load（外部からも GetSerializer() で取得可能）
    ///   PrefabManager   … プレファブ管理
    ///   ObjectSelector  … 選択状態・レイキャスト（外部から GetSelector() で取得可能）
    ///   ModelBrowser    … モデルフォルダ UI        [USE_IMGUI]
    ///   SceneEditorUI   … Inspector / ObjectList  [USE_IMGUI]
    ///   GizmoController … ギズモ描画・Undo/Redo    [USE_IMGUI]
    /// </summary>
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

        // シーンにオブジェクトを配置
        void PlaceObject(const std::string& modelPath);

        // シーンの読み込み
        void LoadScene(const std::string& sceneName);

        // カメラのセット
        void SetCamera(Camera* camera) {
            camera_ = camera;
            selector_.SetCamera(camera);
            motionEditor_.SetCamera(camera);
            colliderLine_.SetCamera(camera_);
			objectManager_->SetCamera(camera);
        }

        //=========================================================================
        // サブシステムアクセッサ
        //=========================================================================
        SceneSerializer& GetSerializer() { return serializer_; }
        ObjectSelector& GetSelector() { return selector_; }
        MotionEditor& GetMotionEditor() { return motionEditor_; }
#ifdef USE_IMGUI
        //GizmoController& GetGizmoController() { return gizmoCtrl_; }
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

        Line   colliderLine_;           // コライダーAABB可視化用
        bool   showColliderDebug_ = true; // コライダー表示フラグ
#ifdef USE_IMGUI
        ColliderDebugViewMode colliderDebugViewMode_ = ColliderDebugViewMode::kAll; // コライダー表示対象フィルタ
#endif

        // コピーしたオブジェクトのIDを保持
        std::vector<int> copyObjectIDs_;
        Vector3 offsetCopyPos_ = { 1.0f,0.0f,0.0f };
        // ── サブシステム ─────────────────────────────────────────
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