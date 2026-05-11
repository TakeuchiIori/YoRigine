#include "ToolbarPanel.h"
#include <Object3D/Object3d.h>
#include "Object3D/ObjectManager.h"
#include "Model.h"
#include "../../Core/MotionSystem.h"
#include "../../Core/Motion.h"
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

	// ============================================================
	// 再生コントロール (1行目)
	// ============================================================
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
				if (context_->currentMotion && ms->GetAnimation() != context_->currentMotion) {
					ms->SetAnimation(context_->currentMotion);
				}
				float savedTime = ms->GetAnimationTime();
				if (savedTime >= ms->GetDuration() || ms->IsFinished()) savedTime = 0.0f;

				if (context_->isLoop) ms->PlayLoop(); else ms->PlayOnce();
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

	ImGui::SameLine(0, 8);
	{
		bool isRev = context_->isReverse;
		if (isRev) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button(isRev ? (std::string(Icon::SyncAlt) + " 逆再生 ON").c_str()
			: (std::string(Icon::SyncAlt) + " 逆再生").c_str())) {
			context_->isReverse = !context_->isReverse;
			if (model && model->GetMotionSystem()) {
				auto* ms = model->GetMotionSystem();
				float spd = ms->GetMotionSpeed();
				ms->SetMotionSpeed(-spd);
				if (context_->isReverse && ms->GetAnimationTime() <= 0.01f) {
					ms->SetAnimationTime(ms->GetDuration());
					context_->scrubTime = ms->GetDuration();
				}
				if (!context_->isReverse && ms->GetAnimationTime() >= ms->GetDuration() - 0.01f) {
					ms->SetAnimationTime(0.0f);
					context_->scrubTime = 0.0f;
				}
			}
			context_->statusMsg = context_->isReverse ? "逆再生モード ON" : "逆再生モード OFF";
		}
		if (isRev) ImGui::PopStyleColor();
	}

	ImGui::SameLine(0, 8);
	{
		bool pp = context_->isPingPong;
		if (pp) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.7f, 1.0f));
		if (ImGui::Button(pp ? "PingPong ON" : "PingPong")) {
			context_->isPingPong = !context_->isPingPong;
			context_->pingPongForward = true;
			context_->statusMsg = context_->isPingPong ? "ピンポン再生 ON" : "ピンポン再生 OFF";
		}
		if (pp) ImGui::PopStyleColor();
	}

	ImGui::SameLine(0, 20);
	ImGui::Checkbox("ボーン表示", &context_->isDrawBone);

	ImGui::SameLine(0, 20);
	if (ImGui::Button("Save/Load...")) {
		context_->showSavePopup = true;
	}

	// ============================================================
	// 2行目: 速度 / トリム / ユーティリティ
	// ============================================================
	ImGui::SetNextItemWidth(120);
	if (ImGui::SliderFloat("再生速度", &context_->playbackSpeed, 0.1f, 3.0f, "x%.2f")) {
		if (model && model->GetMotionSystem()) {
			float sign = context_->isReverse ? -1.0f : 1.0f;
			model->GetMotionSystem()->SetMotionSpeed(sign * context_->playbackSpeed);
		}
	}

	ImGui::SameLine(0, 20);
	ImGui::Checkbox("トリム", &context_->useTrim);
	if (context_->useTrim && context_->currentMotion) {
		float duration = context_->currentMotion->GetDuration();
		if (context_->trimEnd <= 0.0f) context_->trimEnd = duration;
		ImGui::SameLine(0, 8);
		ImGui::SetNextItemWidth(200);
		ImGui::DragFloatRange2("範囲", &context_->trimStart, &context_->trimEnd, 0.01f, 0.0f, duration, "%.3f s", "%.3f s");
	}

	// ============================================================
	// ユーティリティボタン (ミラー / トリム保存)
	// ============================================================
	if (context_->currentMotion) {
		if (ImGui::Button("ミラー生成 (左右反転)")) {
			GenerateMirrorMotion();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("X軸の位置とY/Z回転を反転した\nミラーモーションを生成します");

		if (context_->useTrim) {
			ImGui::SameLine(0, 8);
			if (ImGui::Button("トリム範囲を別モーションとして切り出し")) {
				TrimMotion();
			}
		}

		ImGui::SameLine(0, 8);
		if (ImGui::Button("ピンポン結合して保存")) {
			GeneratePingPongMotion();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("正再生+逆再生を結合した\n1つのモーションを生成します");
	}
#endif
}

void ToolbarPanel::GenerateMirrorMotion()
{
	if (!context_->currentMotion) return;

	Motion mirrored = *context_->currentMotion;
	for (auto& [name, na] : mirrored.animation_.nodeAnimations_) {
		// 位置のX成分を反転
		for (auto& kf : na.translate.keyframes) {
			kf.value.x = -kf.value.x;
		}
		// 回転のY/Z成分を反転 (左右ミラー)
		for (auto& kf : na.rotate.keyframes) {
			kf.value.y = -kf.value.y;
			kf.value.z = -kf.value.z;
		}
	}

	// ボーン名の左右入れ替え (Left<->Right, L<->R)
	std::map<std::string, Motion::NodeAnimation> swapped;
	for (auto& [name, na] : mirrored.animation_.nodeAnimations_) {
		std::string newName = name;
		// Left <-> Right
		auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
			size_t pos = 0;
			while ((pos = s.find(from, pos)) != std::string::npos) {
				s.replace(pos, from.length(), to);
				pos += to.length();
			}
			};
		// 一時的なプレースホルダーを使って相互入れ替え
		std::string temp = newName;
		// Left/Right パターン
		if (temp.find("Left") != std::string::npos || temp.find("Right") != std::string::npos) {
			replaceAll(temp, "Left", "__TEMP_R__");
			replaceAll(temp, "Right", "Left");
			replaceAll(temp, "__TEMP_R__", "Right");
			newName = temp;
		}
		else if (temp.find(".L") != std::string::npos || temp.find(".R") != std::string::npos) {
			replaceAll(temp, ".L", "__TEMP_R__");
			replaceAll(temp, ".R", ".L");
			replaceAll(temp, "__TEMP_R__", ".R");
			newName = temp;
		}
		else if (temp.find("_L_") != std::string::npos || temp.find("_R_") != std::string::npos) {
			replaceAll(temp, "_L_", "__TEMP_R__");
			replaceAll(temp, "_R_", "_L_");
			replaceAll(temp, "__TEMP_R__", "_R_");
			newName = temp;
		}
		swapped[newName] = std::move(na);
	}
	mirrored.animation_.nodeAnimations_ = std::move(swapped);

	const std::string key = "Mirror:" + context_->selectedAnimKey;
	Model::animationCache_[key] = std::move(mirrored);
	context_->selectedAnimKey = key;
	context_->currentMotion = &Model::animationCache_[key];

	// MotionSystemにも同期
	Object3d* target = context_->GetTargetObject();
	if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
		target->GetModel()->GetMotionSystem()->SetAnimation(context_->currentMotion);
	}

	context_->requireTimelineRebuild = true;
	context_->lastAppliedScrubTime = -1.0f;
	context_->statusMsg = "ミラーモーション生成完了";
}

void ToolbarPanel::TrimMotion()
{
	if (!context_->currentMotion || !context_->useTrim) return;
	float tStart = context_->trimStart;
	float tEnd = context_->trimEnd;
	if (tEnd <= tStart) return;

	Motion trimmed;
	Motion* srcMotion = context_->currentMotion;
	float newDuration = tEnd - tStart;
	trimmed.SetDuration(newDuration);

	auto processChannel = [&](const auto& srcKeyframes, auto& dstKeyframes, Motion::InterpolationType interp) {
		if (srcKeyframes.empty()) return;

		// 開始地点の値を計算して追加
		dstKeyframes.push_back({ 0.0f, srcMotion->CalculateValue(srcKeyframes, tStart, interp) });

		for (const auto& kf : srcKeyframes) {
			// トリム範囲内のキーフレームを追加 (時間をトリム開始からのオフセットに変換)
			if (kf.time > tStart && kf.time < tEnd) {
				dstKeyframes.push_back({ kf.time - tStart, kf.value });
			}
		}

		if (newDuration > 0.0f) {
			// 終了地点の値を計算して追加
			dstKeyframes.push_back({ newDuration, srcMotion->CalculateValue(srcKeyframes, tEnd, interp) });
		}
		};

	for (const auto& [name, na] : srcMotion->animation_.nodeAnimations_) {
		Motion::NodeAnimation newNA;
		newNA.interpolationType = na.interpolationType;

		// 各SRTのチャンネルごとに処理を回す
		processChannel(na.translate.keyframes, newNA.translate.keyframes, na.interpolationType);
		processChannel(na.rotate.keyframes, newNA.rotate.keyframes, na.interpolationType);
		processChannel(na.scale.keyframes, newNA.scale.keyframes, na.interpolationType);

		trimmed.animation_.nodeAnimations_[name] = std::move(newNA);
	}

	const std::string key = "Trim:" + context_->selectedAnimKey + "[" +
		std::to_string(tStart) + "-" + std::to_string(tEnd) + "]";
	Model::animationCache_[key] = std::move(trimmed);
	context_->selectedAnimKey = key;
	context_->currentMotion = &Model::animationCache_[key];

	Object3d* target = context_->GetTargetObject();
	if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
		auto* ms = target->GetModel()->GetMotionSystem();
		ms->SetAnimation(context_->currentMotion);
		ms->SetAnimationTime(0.0f);
	}

	context_->requireTimelineRebuild = true;
	context_->lastAppliedScrubTime = -1.0f;
	context_->scrubTime = 0.0f;
	context_->useTrim = false;
	context_->statusMsg = "トリムモーション生成: " + std::to_string(newDuration) + "s";
}

void ToolbarPanel::GeneratePingPongMotion()
{
	if (!context_->currentMotion) return;

	Motion pingpong;
	float originalDuration = context_->currentMotion->GetDuration();
	float newDuration = originalDuration * 2.0f;
	pingpong.SetDuration(newDuration);

	for (const auto& [name, na] : context_->currentMotion->animation_.nodeAnimations_) {
		Motion::NodeAnimation newNA;
		newNA.interpolationType = na.interpolationType;

		auto buildPingPong = [&](const auto& src, auto& dst) {
			// 正方向のキーフレーム
			for (const auto& kf : src) {
				dst.push_back(kf);
			}
			// 逆方向のキーフレーム (時間を反転してオフセット)
			for (auto it = src.rbegin(); it != src.rend(); ++it) {
				auto newKf = *it;
				newKf.time = originalDuration + (originalDuration - it->time);
				// 重複回避: ちょうどoriginalDurationのKFはスキップ
				if (!dst.empty() && std::abs(newKf.time - dst.back().time) < 1e-4f) continue;
				dst.push_back(newKf);
			}
			};

		buildPingPong(na.translate.keyframes, newNA.translate.keyframes);
		buildPingPong(na.rotate.keyframes, newNA.rotate.keyframes);
		buildPingPong(na.scale.keyframes, newNA.scale.keyframes);

		pingpong.animation_.nodeAnimations_[name] = std::move(newNA);
	}

	const std::string key = "PingPong:" + context_->selectedAnimKey;
	Model::animationCache_[key] = std::move(pingpong);
	context_->selectedAnimKey = key;
	context_->currentMotion = &Model::animationCache_[key];

	Object3d* target = context_->GetTargetObject();
	if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
		auto* ms = target->GetModel()->GetMotionSystem();
		ms->SetAnimation(context_->currentMotion);
		ms->SetAnimationTime(0.0f);
	}

	context_->requireTimelineRebuild = true;
	context_->lastAppliedScrubTime = -1.0f;
	context_->scrubTime = 0.0f;
	context_->statusMsg = "ピンポンモーション生成完了 (" + std::to_string(newDuration) + "s)";
}
