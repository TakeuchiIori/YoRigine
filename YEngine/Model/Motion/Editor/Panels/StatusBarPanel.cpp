#include "StatusBarPanel.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void StatusBarPanel::DrawImGui() {
#ifdef USE_IMGUI
	float barH = 20.0f;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
	ImGui::BeginChild("##status", ImVec2(0, barH), false);

	ImGui::SetCursorPosY(3);
	ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "  %s", context_->statusMsg.c_str());

	float hints_x = ImGui::GetContentRegionAvail().x - 400;
	if (hints_x > 200) {
		ImGui::SameLine(hints_x);
		ImGui::TextDisabled("Space=再生  Del=KF削除  Ctrl+Z=元に戻す  Ctrl+Y=やり直す");
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
#endif
}