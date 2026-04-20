#include "ToolbarPanel.h"
#include <Object3D/Object3d.h>
#include "Object3D/ObjectManager.h"
#include "Model.h"
#include "../../Core/MotionSystem.h"
#include <Editor/Icon/EditorIcon.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void ToolbarPanel::DrawImGui()
{
#ifdef USE_IMGUI
	Object3d* target = context_->GetTargetObject();
	Model* model = target ? target->GetModel() : nullptr;

	if (model) {
		ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "[%s]", model->GetName().c_str());
		ImGui::SameLine(0, 12);
	}

	// ------------------------------------------------------------
	// 再生コントロール
	// ------------------------------------------------------------
	if (context_->isPlaying) {
		if (ImGui::Button((std::string(Icon::Pause) + " 一時停止").c_str())) {
			if (model && model->GetMotionSystem()) model->GetMotionSystem()->Stop();
			context_->isPlaying = false;
			context_->statusMsg = "一時停止";
		}
	}
	else {
		if (ImGui::Button((std::string(Icon::Play) + " 再生").c_str())) {
			if (model && model->GetMotionSystem()) {
				auto* ms = model->GetMotionSystem();
				float savedTime = ms->GetAnimationTime();
				if (savedTime >= ms->GetDuration() || ms->IsFinished()) savedTime = 0.0f;

				if (context_->isLoop) target->PlayLoop(); else target->PlayOnce();
				ms->SetAnimationTime(savedTime);
			}
			context_->isPlaying = true;
			context_->statusMsg = "再生 (Play)";
		}
	}

	ImGui::SameLine(0, 3);
	if (ImGui::Button((std::string(Icon::Stop) + " 停止").c_str())) {
		if (model && model->GetMotionSystem()) {
			model->GetMotionSystem()->Stop();
			model->GetMotionSystem()->SetAnimationTime(0.0f);
		}
		context_->isPlaying = false;
		context_->scrubTime = 0.0f;
		context_->statusMsg = "停止 (先頭に戻る)";
	}

	ImGui::SameLine(0, 20);
	ImGui::Checkbox("ボーン表示", &context_->isDrawBone);

	ImGui::SameLine(0, 20);
	if (ImGui::Button("Save/Load...")) {
		context_->showSavePopup = true; // ポップアップ管理パネルへ伝達
	}
#endif
}