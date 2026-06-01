#include "ObjectSelector.h"
#include "PickBuffer.h"

#include "Systems/Input/Input.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"
#include "Editor/Editor.h"
#endif

namespace YoRigine {

    void ObjectSelector::SelectObject(int id, bool multiSelect)
    {
        if (!IsValidId(id)) return;
        if (multiSelect) {
            if (IsSelected(id)) RemoveFromSelection(id);
            else                AddToSelection(id);
        }
        else {
            ClearSelection();
            AddToSelection(id);
        }
    }

    void ObjectSelector::AddToSelection(int id)
    {
        if (!IsValidId(id)) return;
        selectedObjectIds_.insert(id);
        primaryId_ = id;
    }

    void ObjectSelector::RemoveFromSelection(int id)
    {
        selectedObjectIds_.erase(id);
        if (primaryId_ == id)
            primaryId_ = selectedObjectIds_.empty() ? -1 : *selectedObjectIds_.begin();
    }

    void ObjectSelector::ClearSelection()
    {
        selectedObjectIds_.clear();
        primaryId_ = -1;
    }

    bool ObjectSelector::IsSelected(int id) const
    {
        return selectedObjectIds_.find(id) != selectedObjectIds_.end();
    }

    // =============================================================================
    // Update
    // =============================================================================
#ifdef USE_IMGUI
    void ObjectSelector::Update(
        bool          isMouseSelecting,
        const ImVec2& viewportPos,
        const ImVec2& viewportSize)
    {
        if (!isMouseSelecting || !camera_ || !objectManager_) return;
        if (!Editor::GetInstance()->GetShowEditor())           return;
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver())         return;
        // ImGui (ImGuizmo ハンドル含む) がマウスを掴みたがってる間は選択を一切無効化。
        // 「ギズモを最優先」を実現するため、ピック登録より前にここで弾く。
        if (ImGui::GetIO().WantCaptureMouse)                   return;

        ImVec2 mouse = ImGui::GetMousePos();

        // ビューポート外はスキップ
        if (mouse.x < viewportPos.x || mouse.x > viewportPos.x + viewportSize.x ||
            mouse.y < viewportPos.y || mouse.y > viewportPos.y + viewportSize.y) {
            wasMouseDown_ = false;
            pendingMultiSelect_ = false;
            return;
        }

        bool clicked = Input::GetInstance()->IsTriggerMouse(0);

        // ── フェーズ1: クリックされた瞬間 → RequestPick で座標を登録 ──────────
        if (!wasMouseDown_ && clicked) {
            if (pickBuffer_) {
                // ゲームビュー左上を原点とした相対座標に変換して渡す。
                // PickBuffer のビューポートは SetViewport() でゲームビューサイズに
                // 合わせているため、ここもゲームビュー相対座標で渡す必要がある。
                float relX = mouse.x - viewportPos.x;
                float relY = mouse.y - viewportPos.y;

                // PickBuffer にビューポートサイズも伝えてビューポートを更新する
                pickBuffer_->SetViewport(
                    static_cast<uint32_t>(viewportSize.x),
                    static_cast<uint32_t>(viewportSize.y));

                pickBuffer_->RequestPick(relX, relY);
                pendingMultiSelect_ = ImGui::GetIO().KeyShift;
                hasPendingRead_ = true;
            }
        }

        // ── フェーズ2: 前フレームのクリックの結果を読み取る ───────────────────
        if (hasPendingRead_ && pickBuffer_) {
            int pickedId = pickBuffer_->ReadPickResult();
            if (pickedId != -1) {
                // 読み取り時点でギズモが Hover/Using ならピック結果を破棄。
                // フェーズ1のクリック登録 → 結果到着までの 1 フレーム間に
                // ユーザがギズモにマウスを乗せた/掴んだケースをここで救う。
                if (!ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
                    SelectObject(pickedId, pendingMultiSelect_);
                }
                hasPendingRead_ = false;
            }
            else if (!clicked) {
                // クリックがなく ReadPickResult も -1 → 空選択
                // （readbackReady_ が false = まだ結果が来ていない場合は何もしない）
                // readbackReady_ は ReadPickResult 内で false にリセットされるので
                // ここでは何フレームか -1 が続いても待つだけでよい
            }
        }

        wasMouseDown_ = clicked;
    }
#endif

    // =============================================================================
    // PerformGPUPick（外部から直接呼ぶ場合のラッパー、通常は Update() 経由）
    // =============================================================================
# ifdef USE_IMUI
    void ObjectSelector::PerformGPUPick(float screenX, float screenY)
    {
        if (!pickBuffer_) return;
        pickBuffer_->RequestPick(screenX, screenY);
    }

#endif

} // namespace YoRigine