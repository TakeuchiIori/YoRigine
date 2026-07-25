// ===========================================================
// YEditorWidget_Layout.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Layout.h"

namespace YEditorWidget {

void SectionHeader(const char* label)
{
    ImGui::SeparatorText(label);
}

// ── Section ─────────────────────────────────────────────────
Section::Section(const char* label, bool defaultOpen)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen * defaultOpen;
    open_ = ImGui::CollapsingHeader(label, flags);
}

Section::~Section() {}

// ── TreeNode ─────────────────────────────────────────────────
TreeNode::TreeNode(const char* label)
{
    open_ = ImGui::TreeNode(label);
}

TreeNode::~TreeNode()
{
    if (open_) ImGui::TreePop();
}

// ── TabBar ───────────────────────────────────────────────────
TabBar::TabBar(const char* id)
{
    open_ = ImGui::BeginTabBar(id);
}

TabBar::~TabBar()
{
    if (open_) ImGui::EndTabBar();
}

// ── Tab ──────────────────────────────────────────────────────
Tab::Tab(const char* label)
{
    open_ = ImGui::BeginTabItem(label);
}

Tab::~Tab()
{
    if (open_) ImGui::EndTabItem();
}

// ── EditorWindow ─────────────────────────────────────────────
EditorWindow::EditorWindow(const char* title, CommandHistory& history,
                           ImGuiWindowFlags flags)
    : history_(history)
{
    open_ = ImGui::Begin(title, nullptr, flags);
    if (open_) {
        // フォーカス中のウィンドウにだけ Ctrl+Z/Y を流す
        history_.HandleKeyInput();
    }
}

EditorWindow::~EditorWindow()
{
    ImGui::End();
}

} // namespace YEditorWidget
#endif // USE_IMGUI
