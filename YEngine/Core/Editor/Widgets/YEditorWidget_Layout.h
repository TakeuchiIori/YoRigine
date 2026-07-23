#pragma once
// ===========================================================
// YEditorWidget_Layout.h
//
// ImGui レイアウト系ウィジェットの RAII ラッパー。
//
// 使い方:
//   // セクション
//   if (YEditorWidget::Section sec{"Glow"}) { ... }
//
//   // タブ
//   if (YEditorWidget::TabBar bar{"##tabs"}) {
//       if (YEditorWidget::Tab t{"Trail"}) { ... }
//       if (YEditorWidget::Tab t{"Motion"}) { ... }
//   }
//
//   // エディタウィンドウ (Ctrl+Z/Y を自動でこのウィンドウに閉じ込める)
//   if (YEditorWidget::EditorWindow win{"VFX Editor", history_}) { ... }
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Command/CommandHistory.h"

namespace YEditorWidget {

// ── セクション区切りテキスト ──────────────────────────────────
// ImGui::SeparatorText 相当（ラベル付き水平線）
void SectionHeader(const char* label);

// ── CollapsingHeader RAII ─────────────────────────────────────
struct Section
{
    explicit Section(const char* label, bool defaultOpen = true);
    ~Section();
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// ── TreeNode RAII ────────────────────────────────────────────
struct TreeNode
{
    explicit TreeNode(const char* label);
    ~TreeNode();
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// ── TabBar + Tab RAII ────────────────────────────────────────
struct TabBar
{
    explicit TabBar(const char* id);
    ~TabBar();
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

struct Tab
{
    explicit Tab(const char* label);
    ~Tab();
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// ── エディタウィンドウ RAII ───────────────────────────────────
// Begin/End + HandleKeyInput（Ctrl+Z/Y をこのウィンドウにだけ流す）を自動管理する。
// 全エディタはこれを使うことでウィンドウフォーカス中のみ自分の Undo が動く。
struct EditorWindow
{
    EditorWindow(const char* title, CommandHistory& history,
                 ImGuiWindowFlags flags = 0);
    ~EditorWindow();
    explicit operator bool() const { return open_; }
private:
    bool            open_;
    CommandHistory& history_;
};

} // namespace YEditorWidget
#endif // USE_IMGUI
