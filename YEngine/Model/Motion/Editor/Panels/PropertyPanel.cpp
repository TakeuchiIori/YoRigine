#include "PropertyPanel.h"
#include "../../Core/Motion.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void PropertyPanel::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("BONE TRANSFORM  (位置・回転・拡縮)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (context_->selBone.empty()) {
			ImGui::TextDisabled("左のリストからボーンを選択してください");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "選択中: %s", context_->selBone.c_str());
			ImGui::Separator();

			// 値が変更されたら Joint に同期する
			bool changed = false;
			changed |= ImGui::DragFloat3("位置 (T)", context_->editT, 0.01f);
			changed |= ImGui::DragFloat3("回転 (R)", context_->editR, 0.5f);
			changed |= ImGui::DragFloat3("拡縮 (S)", context_->editS, 0.01f, 0.001f, 100.0f);

			if (changed && context_->SyncBufferToJoint) {
				context_->SyncBufferToJoint();
			}

			ImGui::Spacing();

			if (context_->currentMotion) {
				ImGui::Text("現在時刻: %.4f 秒", context_->scrubTime);
				if (ImGui::Button("[ KF ] 選択ボーンのキーフレームを打つ", ImVec2(-1, 0))) {
					// （※実際にはここに InsertKeyframe の呼び出し処理を書く）
					if (context_->AddKeyframe) {
						context_->AddKeyframe(context_->selBone, context_->scrubTime);
					}
					context_->statusMsg = "KF 挿入: " + context_->selBone;
				}
			}
		}
	}
#endif
}