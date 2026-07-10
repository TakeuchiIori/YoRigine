#include "MenuBarPanel.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void MenuBarPanel::DrawImGui() {
#ifdef USE_IMGUI
	if (!ImGui::BeginMenuBar()) return;

	if (ImGui::BeginMenu("ファイル (File)")) {
		if (ImGui::MenuItem("元アニメーション読み込み", "")) context_->showSourceAnimationPopup = true;
		if (ImGui::MenuItem("バイナリ保存/読込", ""))  context_->showSavePopup = true;
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("編集 (Edit)")) {
		if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, context_->history.CanUndo())) context_->history.Undo();
		if (ImGui::MenuItem("やり直す", "Ctrl+Y", false, context_->history.CanRedo())) context_->history.Redo();
		ImGui::Separator();
		if (ImGui::MenuItem("履歴をクリア")) context_->history.Clear();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("表示 (View)")) {
		ImGui::SliderFloat("タイムライン高さ", &context_->timelineH, 80.0f, 500.0f, "%.0f px");
		ImGui::SliderFloat("ボーンリスト幅", &context_->bonePanelW, 80.0f, 450.0f, "%.0f px");
		ImGui::SliderFloat("ズーム (px/秒)", &context_->timelineZoom, 20.0f, 400.0f, "%.0f");
		ImGui::EndMenu();
	}

	ImGui::Separator();
	if (!context_->history.CanUndo()) ImGui::BeginDisabled();
	if (ImGui::SmallButton(" << ")) { context_->history.Undo(); context_->statusMsg = "元に戻す"; }
	if (!context_->history.CanUndo()) ImGui::EndDisabled();

	ImGui::SameLine(0, 2);
	if (!context_->history.CanRedo()) ImGui::BeginDisabled();
	if (ImGui::SmallButton(" >> ")) { context_->history.Redo(); context_->statusMsg = "やり直す"; }
	if (!context_->history.CanRedo()) ImGui::EndDisabled();

	ImGui::EndMenuBar();
#endif
}
