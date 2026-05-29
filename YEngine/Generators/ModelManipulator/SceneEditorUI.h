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

    enum class ColliderDebugViewMode {
        kAll = 0,          // 全オブジェクトを表示
        kEnabledOnly,      // colliderEnabled == true のみ表示
        kSelectedOnly,     // 選択中オブジェクトのみ表示
    };

    /// <summary>
    /// ObjectList / TransformControls / DuplicateWindow / PrefabWindow の
    /// ImGui 描画をまとめたクラス。
    /// ロジックは各サブシステムへ委譲し、UI の組み立てのみを担う。
    /// </summary>
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
        void SetColliderDebugFlag(bool* flag) { showColliderDebug_ = flag; } // ModelManipulatorのフラグを参照
        void SetColliderDebugViewMode(ColliderDebugViewMode* mode) { colliderDebugViewMode_ = mode; } // ModelManipulatorの表示モードを参照

        /// オブジェクト配置要求（ModelManipulator::PlaceObject を渡す）
        void SetPlaceCallback(std::function<void(const std::string&)> cb) { placeCallback_ = cb; }
        /// シーン保存要求
        void SetSaveCallback(std::function<void()> cb) { saveCallback_ = cb; }
        /// シーン読み込み要求
        void SetLoadCallback(std::function<void()> cb) { loadCallback_ = cb; }

        // ── ウィンドウ表示フラグ ──────────────────────────────────
        void SetShowObjectList(bool v) { showObjectList_ = v; }
        void SetShowTransformControls(bool v) { showTransformControls_ = v; }
        void SetShowDuplicateWindow(bool v) { showDuplicateWindow_ = v; }
        void SetShowPrefabWindow(bool v) { showPrefabWindow_ = v; }

        bool* GetShowObjectListPtr() { return &showObjectList_; }
        bool* GetShowTransformControlsPtr() { return &showTransformControls_; }
        bool* GetShowDuplicateWindowPtr() { return &showDuplicateWindow_; }
        bool* GetShowPrefabWindowPtr() { return &showPrefabWindow_; }
        bool* GetShowColliderTemplatesPtr() { return &showColliderTemplates_; }

        // ── ImGui 描画（DrawImGui() から呼ぶ） ───────────────────
        void DrawObjectList();
        void DrawTransformControls();
        void DrawDuplicateWindow();
        void DrawPrefabWindow();
        void DrawColliderTemplates(); // コライダーテンプレート一覧ウィンドウ

        // ── メニューバー拡張 ──────────────────────────────────────
        void DrawMenuBar();

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
        bool showColliderTemplates_ = false; // コライダーテンプレート一覧
        bool* showColliderDebug_ = nullptr; // ModelManipulator::showColliderDebug_ への参照
        ColliderDebugViewMode* colliderDebugViewMode_ = nullptr; // ModelManipulator::colliderDebugViewMode_ への参照

        // 複製ウィンドウ用
        Vector3 duplicateOffset_ = { 1.0f, 0.0f, 0.0f };
        int     duplicateCount_ = 1;
        bool    duplicateKeepParent_ = false;

        // プレファブウィンドウ用
        std::string selectedPrefabName_;
        char        prefabNameBuf_[64] = {};
    };

} // namespace YoRigine

#endif // USE_IMGUI