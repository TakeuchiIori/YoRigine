#include "ToolbarPanel.h"
#include <Object3D/Object3d.h>
#include "Object3D/ObjectManager.h"
#include "Model.h"
#include "../../Core/MotionSystem.h"
#include "../../Core/Motion.h"

#include <IconsFontAwesome5.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

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
		if (ImGui::Button((std::string(ICON_FA_PAUSE) + " 一時停止").c_str())) {
			if (model && model->GetMotionSystem()) model->GetMotionSystem()->Stop();
			context_->isPlaying = false;
			context_->statusMsg = "一時停止";
		}
	}
	else {
		if (ImGui::Button((std::string(ICON_FA_PLAY) + " 再生").c_str())) {
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
	if (ImGui::Button((std::string(ICON_FA_STOP) + " 停止").c_str())) {
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
		if (ImGui::Button(isRev ? (std::string(ICON_FA_SYNC_ALT) + " 逆再生 ON").c_str()
			: (std::string(ICON_FA_SYNC_ALT) + " 逆再生").c_str())) {
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
	if (ImGui::DragFloat("再生速度", &context_->playbackSpeed, 0.1f, 3.0f)) {
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

		ImGui::Separator();
		ImGui::TextUnformatted("区間移動 焼き込み");
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(110);
		ImGui::InputInt("開始f##offsetStart", &offsetStartFrame_);
		ImGui::SameLine(0, 8);
		ImGui::SetNextItemWidth(110);
		ImGui::InputInt("終了f##offsetEnd", &offsetEndFrame_);
		ImGui::SameLine(0, 8);
		ImGui::SetNextItemWidth(220);
		ImGui::DragFloat3("移動量XYZ##offsetTranslate", offsetTranslate_, 0.01f);
		ImGui::SameLine(0, 8);
		const bool canBakeOffset = !context_->selBone.empty();
		if (!canBakeOffset) ImGui::BeginDisabled();
		if (ImGui::Button("選択ボーンへ焼き込み")) {
			BakeTranslateOffsetRange();
		}
		if (!canBakeOffset) ImGui::EndDisabled();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("開始直前/開始/終了/終了直後にTranslateキーを作り、指定区間だけ移動量を足します");
		}
		ImGui::EndGroup();
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

void ToolbarPanel::BakeTranslateOffsetRange()
{
	if (!context_->currentMotion || context_->selBone.empty()) return;

	constexpr int kFps = 60;
	Motion* motion = context_->currentMotion;
	auto nodeIt = motion->animation_.nodeAnimations_.find(context_->selBone);
	if (nodeIt == motion->animation_.nodeAnimations_.end()) {
		context_->statusMsg = "区間移動失敗: ボーンが見つかりません";
		return;
	}

	const int totalFrames = std::max(0, static_cast<int>(motion->GetDuration() * kFps));
	int startFrame = std::clamp(offsetStartFrame_, 0, totalFrames);
	int endFrame = std::clamp(offsetEndFrame_, 0, totalFrames);
	if (endFrame < startFrame) std::swap(startFrame, endFrame);

	const float unitScale = std::max(0.0001f, context_->translateDisplayScale);
	const Vector3 offset = {
		offsetTranslate_[0] * unitScale,
		offsetTranslate_[1] * unitScale,
		offsetTranslate_[2] * unitScale
	};
	if (std::abs(offset.x) < 1e-6f && std::abs(offset.y) < 1e-6f && std::abs(offset.z) < 1e-6f) {
		context_->statusMsg = "区間移動なし: 移動量が0です";
		return;
	}

	const auto oldAnims = motion->animation_.nodeAnimations_;
	auto newAnims = oldAnims;
	auto& targetNode = newAnims[context_->selBone];

	auto insertOrReplace = [](auto& keyframes, float time, const Vector3& value) {
		auto it = std::find_if(keyframes.begin(), keyframes.end(),
			[time](const auto& kf) { return std::abs(kf.time - time) < 1e-4f; });
		if (it != keyframes.end()) {
			it->value = value;
		}
		else {
			keyframes.push_back({ time, value });
		}
		std::sort(keyframes.begin(), keyframes.end(),
			[](const auto& a, const auto& b) { return a.time < b.time; });
		};

	const float startTime = startFrame / static_cast<float>(kFps);
	const float endTime = endFrame / static_cast<float>(kFps);
	const Motion::InterpolationType interp = targetNode.interpolationType;

	if (startFrame > 0) {
		const float beforeTime = (startFrame - 1) / static_cast<float>(kFps);
		const Vector3 beforeValue = motion->CalculateValue(nodeIt->second.translate.keyframes, beforeTime, interp);
		insertOrReplace(targetNode.translate.keyframes, beforeTime, beforeValue);
	}

	const Vector3 startValue = motion->CalculateValue(nodeIt->second.translate.keyframes, startTime, interp);
	const Vector3 endValue = motion->CalculateValue(nodeIt->second.translate.keyframes, endTime, interp);
	insertOrReplace(targetNode.translate.keyframes, startTime, startValue + offset);

	for (const auto& kf : nodeIt->second.translate.keyframes) {
		if (kf.time > startTime + 1e-4f && kf.time < endTime - 1e-4f) {
			insertOrReplace(targetNode.translate.keyframes, kf.time, kf.value + offset);
		}
	}

	insertOrReplace(targetNode.translate.keyframes, endTime, endValue + offset);

	if (endFrame < totalFrames) {
		const float afterTime = (endFrame + 1) / static_cast<float>(kFps);
		const Vector3 afterValue = motion->CalculateValue(nodeIt->second.translate.keyframes, afterTime, interp);
		insertOrReplace(targetNode.translate.keyframes, afterTime, afterValue);
	}

	context_->history.Execute(MakeLambdaCommand("区間移動焼き込み: " + context_->selBone,
		[this, newAnims]() {
			if (!context_->currentMotion) return;
			context_->currentMotion->animation_.nodeAnimations_ = newAnims;
			context_->requireTimelineRebuild = true;
			context_->lastAppliedScrubTime = -1.0f;
		},
		[this, oldAnims]() {
			if (!context_->currentMotion) return;
			context_->currentMotion->animation_.nodeAnimations_ = oldAnims;
			context_->requireTimelineRebuild = true;
			context_->lastAppliedScrubTime = -1.0f;
		}
	));

	Object3d* target = context_->GetTargetObject();
	if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
		auto* ms = target->GetModel()->GetMotionSystem();
		ms->SetAnimation(context_->currentMotion);
		ms->SetAnimationTime(context_->scrubTime);
	}

	context_->requireTimelineRebuild = true;
	context_->lastAppliedScrubTime = -1.0f;
	context_->statusMsg = "区間移動焼き込み: " + context_->selBone
		+ " [" + std::to_string(startFrame) + "f-" + std::to_string(endFrame) + "f]";
}
