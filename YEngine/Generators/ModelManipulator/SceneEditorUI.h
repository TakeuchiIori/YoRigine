#pragma once

#ifdef USE_IMGUI

// C++
#include <string>
#include <functional>

// Engine
#include <Object3D/ObjectManager.h>
#include <Debugger/Gizmo/GizmoController.h>

// Forward
namespace YoRigine {
    class ObjectSelector;
    class PrefabManager;
    class SceneSerializer;
}

namespace YoRigine {

    class SceneEditorUI
    {
    public:
        SceneEditorUI() = default;
        ~SceneEditorUI() = default;

        // ── 依存注入 ─────────────────────────────────────────────
        void SetObjectManager(ObjectManager* mgr) { objectManager_ = mgr; }
        void SetSelector(ObjectSelector* sel) { selector_ = sel; }
        void SetPrefabManager(PrefabManager* pm) { prefabMgr_ = pm; }
        void SetSerializer(SceneSerializer* s) { serializer_ = s; }
        void SetGizmoController(GizmoController* gc) { gizmoCtrl_ = gc; }

        // ModelManipulator 側のフラグ参照
        void SetColliderDebugFlag(bool* flag) { showColliderDebug_ = flag; }
        void SetColliderSelectedOnlyFlag(bool* flag) { showColliderSelectedOnly_ = flag; }
        void SetBroadPhaseGridFlag(bool* flag) { showBroadPhaseGrid_ = flag; }
        void SetBroadPhaseGridRadius(float* val) { broadPhaseGridDrawRadius_ = val; }
        void SetDrawFrustumCullingFlag(bool* flag) { drawFrustumCulling_ = flag; }

        void SetPlaceCallback(std::function<void(const std::string&)> cb) { placeCallback_ = cb; }
        void SetSaveCallback(std::function<void()> cb) { saveCallback_ = cb; }
        void SetLoadCallback(std::function<void()> cb) { loadCallback_ = cb; }

        // ── ウィンドウ表示フラグ ──────────────────────────────────
        void SetShowObjectList(bool v) { showObjectList_ = v; }
        void SetShowTransformControls(bool v) { showTransformControls_ = v; }
        void SetShowDuplicateWindow(bool v) { showDuplicateWindow_ = v; }
        void SetShowPrefabWindow(bool v) { showPrefabWindow_ = v; }

        bool* GetShowObjectListPtr() { return &showObjectList_; }
        bool  IsObjectListShown() const { return showObjectList_; }
        bool* GetShowTransformControlsPtr() { return &showTransformControls_; }
        bool* GetShowDuplicateWindowPtr() { return &showDuplicateWindow_; }
        bool* GetShowPrefabWindowPtr() { return &showPrefabWindow_; }
        bool* GetShowColliderTemplatesPtr() { return &showColliderTemplates_; }

        void DrawObjectList();
        void DrawTransformControls();
        void DrawDuplicateWindow();
        void DrawPrefabWindow();
        void DrawColliderTemplates();

        void DrawMenuBar();

        // ── 配置補助 (メニューから呼ばれる) ──────────────────────────
        // 選択中オブジェクトを真下方向に Raycast して地面/物体表面に吸着させる
        void SnapSelectedToSurface();

    private:
        float DegToRad(float d) const { return d * (3.14159265359f / 180.0f); }
        float RadToDeg(float r) const { return r * (180.0f / 3.14159265359f); }

        // ── 依存 ─────────────────────────────────────────────────
        ObjectManager* objectManager_ = nullptr;
        ObjectSelector* selector_ = nullptr;
        PrefabManager* prefabMgr_ = nullptr;
        SceneSerializer* serializer_ = nullptr;
        GizmoController* gizmoCtrl_ = nullptr;

        std::function<void(const std::string&)> placeCallback_;
        std::function<void()> saveCallback_;
        std::function<void()> loadCallback_;

        // ── UI 状態 ───────────────────────────────────────────────
        bool showObjectList_ = true;
        bool showTransformControls_ = true;
        bool showDuplicateWindow_ = false;
        bool showPrefabWindow_ = false;
        bool showColliderTemplates_ = false;

        // コライダーデバッグ描画（ModelManipulator のフラグ参照）
        bool*  showColliderDebug_ = nullptr;
        bool*  showColliderSelectedOnly_ = nullptr;
        bool*  showBroadPhaseGrid_ = nullptr;
        float* broadPhaseGridDrawRadius_ = nullptr;
        bool*  drawFrustumCulling_ = nullptr;

        Vector3 duplicateOffset_ = { 1.0f, 0.0f, 0.0f };
        int     duplicateCount_ = 1;
        bool    duplicateKeepParent_ = false;

        // コライダー自動フィット時のマージン (1.0 等倍 / 1.05 5%拡大)
        float   colliderFitMargin_ = 1.05f;

        std::string selectedPrefabName_;
        char        prefabNameBuf_[64] = {};
    };

} // namespace YoRigine

#endif // USE_IMGUI