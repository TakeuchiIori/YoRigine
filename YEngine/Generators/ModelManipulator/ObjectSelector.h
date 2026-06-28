#pragma once

#include <set>
#include <cmath>
#include <algorithm>

#include <Object3D/ObjectManager.h>
#include <Systems/Camera/Camera.h>
#include "Vector3.h"
#include "Matrix4x4.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace YoRigine {

    class PickBuffer;

    /// <summary>
    /// オブジェクトの選択状態管理・GPU ピック選択を担う。
    ///
    /// ■ 2フェーズ Pick の流れ
    ///   フレームN:   クリック検出 → RequestPick(x, y) → hasPendingRead_ = true
    ///   フレームN+1: ReadPickResult() → objectID → SelectObject()
    ///
    ///   1フレームのラグがあるが、エディター操作では実用上問題なし。
    ///
    /// ■ 座標について
    ///   ImGui::GetMousePos() が返すのはすでにウィンドウ全体座標なので、
    ///   そのまま RequestPick に渡せばよい。
    ///   PickBuffer の RT はウィンドウ全体サイズで作られているので一致する。
    /// </summary>
    class ObjectSelector
    {
    public:
        ObjectSelector() = default;
        ~ObjectSelector() = default;

        void SetObjectManager(ObjectManager* mgr) { objectManager_ = mgr; }
        void SetCamera(Camera* camera) { camera_ = camera; }
        void SetPickBuffer(PickBuffer* pb) { pickBuffer_ = pb; }

        void SelectObject(int objectId, bool multiSelect);
        void AddToSelection(int objectId);
        void RemoveFromSelection(int objectId);
        void ClearSelection();

        bool IsSelected(int objectId) const;
        bool HasSelection() const { return !selectedObjectIds_.empty(); }

        int  GetPrimaryId() const { return primaryId_; }
        const std::set<int>& GetSelectedIds() const { return selectedObjectIds_; }

#ifdef USE_IMGUI
        void Update(
            bool          isMouseSelecting,
            const ImVec2& viewportPos,
            const ImVec2& viewportSize);

        void PerformGPUPick(float screenX, float screenY);
#endif
    private:
        bool IsValidId(int id) const {
            return objectManager_ && objectManager_->GetObjectById(id) != nullptr;
        }

        ObjectManager* objectManager_ = nullptr;
        Camera* camera_ = nullptr;
        PickBuffer* pickBuffer_ = nullptr;

        std::set<int>  selectedObjectIds_;
        int            primaryId_ = -1;
        bool           wasMouseDown_ = false;

        // 2フェーズ Pick 用
        bool hasPendingRead_ = false; // RequestPick 済み・ReadPickResult 待ち
        bool pendingMultiSelect_ = false; // クリック時の Ctrl 状態を保持
    };

} // namespace YoRigine