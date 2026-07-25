#pragma once
// ===========================================================
// YEditorWidget_Scope.h  (header-only)
//
// UndoRedo 連携用 RAII スコープ。
//
// 【ドラッグ操作のように連続変更が走るケース（推奨パターン）】
//
//   // エディタのメンバとして持つ
//   std::optional<YEditorWidget::ChangeScope<MyParams>> editScope_;
//
//   // Draw() 内：
//   YEditorWidget::DragFloat("Speed", params_.speed);
//   if (ImGui::IsItemActivated())
//       editScope_.emplace(params_, history_);          // ドラッグ開始で before を保存
//   if (ImGui::IsItemDeactivatedAfterEdit() && editScope_) {
//       editScope_->Commit("Speed");                    // ドラッグ終了で履歴に積む
//       editScope_.reset();
//   }
//
// 【コンボ等の即時確定ケース】
//
//   MyParams before = params_;
//   if (YEditorWidget::EnumCombo("Blend", params_.blend, kNames)) {
//       YEditorWidget::CommitChange("Blend Mode", params_, before, history_);
//   }
//
// ===========================================================
#ifdef USE_IMGUI
#include "Core/Editor/Command/CommandHistory.h"
#include <string>

namespace YEditorWidget {

// ── CommitChange ─────────────────────────────────────────────
// T のコピーを before / after として LambdaCommand を生成し Execute する。
// target は参照で保持するためエディタのデータより先に破棄されないこと。
template <typename T>
void CommitChange(const char* name, T& target, const T& before,
                  CommandHistory& history)
{
    T after = target;
    history.Execute(MakeLambdaCommand(
        std::string(name),
        [after, &target]() mutable { target = after; },
        [before, &target]() mutable { target = before; }
    ));
}

// ── ChangeScope ──────────────────────────────────────────────
// 構築時点の T スナップショットを保持し、Commit() で CommitChange を呼ぶ。
// 主にドラッグ操作の「開始〜終了」をまたいで before を保持するために使う。
template <typename T>
class ChangeScope
{
public:
    ChangeScope(T& target, CommandHistory& history)
        : target_(target)
        , before_(target)
        , history_(history)
    {}

    // ドラッグ終了時など変更が確定したタイミングで呼ぶ
    void Commit(const char* name)
    {
        CommitChange(name, target_, before_, history_);
        // 続けてドラッグされる可能性があるので before を更新する
        before_ = target_;
    }

    // まだ確定していない変更を元に戻す（ESC キャンセル用）
    void Revert()
    {
        target_ = before_;
    }

    const T& Before() const { return before_; }

private:
    T&             target_;
    T              before_;
    CommandHistory& history_;
};

} // namespace YEditorWidget
#endif // USE_IMGUI
