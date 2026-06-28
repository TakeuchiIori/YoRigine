#pragma once

// C++
#include <memory>
#include <vector>
#include <string>
#include <functional>

#ifdef USE_IMGUI
#include <imgui.h>
#endif>

/// <summary>
/// コマンドの基底インターフェース
/// すべての操作はこれを継承して Execute / Undo を実装する
/// </summary>
class ICommand
{
public:
    virtual ~ICommand() = default;

    // 操作を実行する
    virtual void Execute() = 0;

    // 操作を取り消す
    virtual void Undo() = 0;

    // デバッグ用の操作名（ImGuiの履歴表示に使用）
    virtual const char* GetName() const = 0;
};

/// <summary>
/// ラムダ式で手軽にコマンドを作れる汎用実装
/// 
/// 使用例:
///   history.Execute(MakeLambdaCommand("Move Joint",
///       [=]() { joint->SetTransform(newTr); },
///       [=]() { joint->SetTransform(oldTr); }
///   ));
/// </summary>
class LambdaCommand : public ICommand
{
public:
    LambdaCommand(
        std::string name,
        std::function<void()> execute,
        std::function<void()> undo)
        : name_(std::move(name))
        , executeFn_(std::move(execute))
        , undoFn_(std::move(undo))
    {
    }

    void Execute() override { executeFn_(); }
    void Undo()    override { undoFn_(); }
    const char* GetName() const override { return name_.c_str(); }

private:
    std::string name_;
    std::function<void()> executeFn_;
    std::function<void()> undoFn_;
};

// 生成ヘルパー
inline std::unique_ptr<LambdaCommand> MakeLambdaCommand(
    std::string name,
    std::function<void()> execute,
    std::function<void()> undo)
{
    return std::make_unique<LambdaCommand>(
        std::move(name), std::move(execute), std::move(undo));
}

/// <summary>
/// Undo / Redo 履歴を管理するクラス
/// 
/// 使い方:
///   CommandHistory history;
///   history.Execute(MakeLambdaCommand(...));   // 操作を実行＆履歴に積む
///   history.Undo();                            // 一つ戻す
///   history.Redo();                            // 一つ進む
///   history.Clear();                           // 履歴を全消去
/// 
/// ImGuiでキーバインドと組み合わせる場合:
///   if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
///       if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) history.Undo();
///       if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) history.Redo();
///   }
/// </summary>
class CommandHistory
{
public:
    static constexpr size_t kMaxHistory = 100; // 最大履歴数

    /// <summary>
    /// コマンドを実行し履歴に追加する
    /// Redo スタックはクリアされる
    /// </summary>
    void Execute(std::unique_ptr<ICommand> cmd)
    {
        cmd->Execute();

        undoStack_.push_back(std::move(cmd));
        redoStack_.clear();

        // 上限を超えたら古いものを削除
        if (undoStack_.size() > kMaxHistory) {
            undoStack_.erase(undoStack_.begin());
        }
    }

    /// <summary>
    /// 直前の操作を取り消す
    /// </summary>
    void Undo()
    {
        if (undoStack_.empty()) return;

        auto& cmd = undoStack_.back();
        cmd->Undo();
        redoStack_.push_back(std::move(cmd));
        undoStack_.pop_back();
    }

    /// <summary>
    /// 取り消した操作をやり直す
    /// </summary>
    void Redo()
    {
        if (redoStack_.empty()) return;

        auto& cmd = redoStack_.back();
        cmd->Execute();
        undoStack_.push_back(std::move(cmd));
        redoStack_.pop_back();
    }

    // Undo可能か
    bool CanUndo() const { return !undoStack_.empty(); }

    // Redo可能か
    bool CanRedo() const { return !redoStack_.empty(); }

    // 履歴を全消去
    void Clear()
    {
        undoStack_.clear();
        redoStack_.clear();
    }

    // Undoスタック（ImGui表示用）
    const std::vector<std::unique_ptr<ICommand>>& GetUndoStack() const { return undoStack_; }

    // Redoスタック（ImGui表示用）
    const std::vector<std::unique_ptr<ICommand>>& GetRedoStack() const { return redoStack_; }

    /// <summary>
    /// ImGuiウィンドウがフォーカス中のとき Ctrl+Z / Ctrl+Y を処理する
    /// 各エディタの ShowEditor() 内で呼ぶ
    /// </summary>
    void HandleKeyInput()
    {
#ifdef USE_IMGUI
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;

        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) { Undo(); }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) { Redo(); }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z | ImGuiMod_Shift)) { Redo(); } // Ctrl+Shift+Z でも Redo
#endif
    }

    /// <summary>
    /// Undo / Redo ボタンと履歴リストを ImGui で描画する
    /// </summary>
    void DrawImGui()
    {
#ifdef USE_IMGUI
        bool canUndo = CanUndo();
        bool canRedo = CanRedo();

        if (!canUndo) ImGui::BeginDisabled();
        if (ImGui::Button("Undo") || (canUndo && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))) {
            Undo();
        }
        if (!canUndo) ImGui::EndDisabled();

        ImGui::SameLine();

        if (!canRedo) ImGui::BeginDisabled();
        if (ImGui::Button("Redo")) {
            Redo();
        }
        if (!canRedo) ImGui::EndDisabled();

        // 履歴リスト（上が新しい）
        if (ImGui::TreeNode("History")) {
            // Redo スタック（薄く表示）
            for (auto it = redoStack_.rbegin(); it != redoStack_.rend(); ++it) {
                ImGui::TextDisabled("  [Redo] %s", (*it)->GetName());
            }
            // Undo スタック
            for (auto it = undoStack_.rbegin(); it != undoStack_.rend(); ++it) {
                ImGui::TextUnformatted((*it)->GetName());
            }
            ImGui::TreePop();
        }
#endif
    }

private:
    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
};