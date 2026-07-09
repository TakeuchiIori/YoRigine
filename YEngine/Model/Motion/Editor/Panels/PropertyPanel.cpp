#include "PropertyPanel.h"
#include "../../Core/Motion.h"
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

static constexpr float kPi = 3.14159265f;

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
			context_->translateDisplayScale = std::max(0.0001f, context_->translateDisplayScale);
			float displayT[3] = {
				context_->editT[0] / context_->translateDisplayScale,
				context_->editT[1] / context_->translateDisplayScale,
				context_->editT[2] / context_->translateDisplayScale
			};
			if (ImGui::DragFloat3("位置 (T)", displayT, 0.01f)) {
				context_->editT[0] = displayT[0] * context_->translateDisplayScale;
				context_->editT[1] = displayT[1] * context_->translateDisplayScale;
				context_->editT[2] = displayT[2] * context_->translateDisplayScale;
				changed = true;
			}
			changed |= ImGui::DragFloat3("回転 (R)", context_->editR, 0.5f);
			changed |= ImGui::DragFloat3("拡縮 (S)", context_->editS, 0.01f, 0.001f, 100.0f);

			if (changed && context_->SyncBufferToJoint) {
				context_->SyncBufferToJoint();
			}

			ImGui::Spacing();
			{
				bool canRestoreLive = context_->hasLiveBoneOriginal && context_->liveOriginalBone == context_->selBone;
				if (!canRestoreLive) ImGui::BeginDisabled();
				if (ImGui::Button("一時編集を元に戻す", ImVec2(-1, 0))) {
					if (context_->RestoreLiveBoneOriginal) {
						context_->RestoreLiveBoneOriginal();
					}
				}
				if (!canRestoreLive) ImGui::EndDisabled();
			}

			ImGui::Spacing();

			if (context_->currentMotion) {
				ImGui::Text("現在時刻: %.4f 秒", context_->scrubTime);
				if (ImGui::Button("[ KF ] 選択ボーンのキーフレームを打つ", ImVec2(-1, 0))) {
					if (context_->AddKeyframe) {
						context_->AddKeyframe(context_->selBone, context_->scrubTime);
					}
					context_->statusMsg = "KF 挿入: " + context_->selBone;
				}

				ImGui::Spacing();
				ImGui::Separator();

				// ============================================================
				// コピー & ペースト
				// ============================================================
				if (ImGui::Button("コピー (Ctrl+C)", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4, 0))) {
					auto& cb = context_->clipboard;
					cb.hasData = true;
					cb.translate = { context_->editT[0], context_->editT[1], context_->editT[2] };
					cb.rotate = EulerToQuaternion({ context_->editR[0] * (kPi / 180), context_->editR[1] * (kPi / 180), context_->editR[2] * (kPi / 180) });
					cb.scale = { context_->editS[0], context_->editS[1], context_->editS[2] };
					cb.sourceBone = context_->selBone;
					cb.sourceTime = context_->scrubTime;
					context_->statusMsg = "コピー: " + context_->selBone + " @ " + std::to_string(context_->scrubTime) + "s";
				}
				ImGui::SameLine(0, 8);
				{
					bool canPaste = context_->clipboard.hasData;
					if (!canPaste) ImGui::BeginDisabled();
					if (ImGui::Button("ペースト (Ctrl+V)", ImVec2(-1, 0))) {
						auto& cb = context_->clipboard;
						// クリップボードの値を編集バッファに展開
						context_->editT[0] = cb.translate.x; context_->editT[1] = cb.translate.y; context_->editT[2] = cb.translate.z;
						Vector3 euler = QuaternionToEuler(cb.rotate);
						context_->editR[0] = euler.x * (180 / kPi); context_->editR[1] = euler.y * (180 / kPi); context_->editR[2] = euler.z * (180 / kPi);
						context_->editS[0] = cb.scale.x; context_->editS[1] = cb.scale.y; context_->editS[2] = cb.scale.z;
						// ジョイントに反映
						if (context_->SyncBufferToJoint) context_->SyncBufferToJoint();
						context_->statusMsg = "ペースト: " + cb.sourceBone + " -> " + context_->selBone;
					}
					if (!canPaste) ImGui::EndDisabled();
				}

				// ペースト＋KF挿入のワンボタン
				{
					bool canPaste = context_->clipboard.hasData;
					if (!canPaste) ImGui::BeginDisabled();
					if (ImGui::Button("ペースト + KF挿入", ImVec2(-1, 0))) {
						auto& cb = context_->clipboard;
						context_->editT[0] = cb.translate.x; context_->editT[1] = cb.translate.y; context_->editT[2] = cb.translate.z;
						Vector3 euler = QuaternionToEuler(cb.rotate);
						context_->editR[0] = euler.x * (180 / kPi); context_->editR[1] = euler.y * (180 / kPi); context_->editR[2] = euler.z * (180 / kPi);
						context_->editS[0] = cb.scale.x; context_->editS[1] = cb.scale.y; context_->editS[2] = cb.scale.z;
						if (context_->SyncBufferToJoint) context_->SyncBufferToJoint();
						if (context_->AddKeyframe) context_->AddKeyframe(context_->selBone, context_->scrubTime);
						context_->statusMsg = "ペースト + KF挿入: " + context_->selBone;
					}
					if (!canPaste) ImGui::EndDisabled();
				}

				// クリップボード状態表示
				if (context_->clipboard.hasData) {
					ImGui::TextDisabled("クリップボード: %s @ %.3fs",
						context_->clipboard.sourceBone.c_str(), context_->clipboard.sourceTime);
				}
			}
		}
	}
#endif
}
