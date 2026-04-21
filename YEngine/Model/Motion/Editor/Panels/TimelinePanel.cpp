#include "TimelinePanel.h"
#include "../../Core/Motion.h"
#include "Object3D/ObjectManager.h"
#include "Model.h"
#include "../../Core/MotionSystem.h"
#include <set>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Editor/Icon/EditorIcon.h>

void TimelinePanel::Initialize(MotionEditorContext* context)
{
	context_ = context;

	// ── 概要トラックの一括移動コールバック ──
	// サマリーキーがドラッグされた時、全ボーン・全チャンネルを一括移動する
	dopeSheet_.SetSummaryKeyMovedCallback([this](int fromFrame, int delta) {
		OnSummaryKeyMoved(fromFrame, delta);
		});

	// ── シーク時のコールバック設定（1回だけ登録）──
	dopeSheet_.SetSeekCallback([this](int frame) {
		context_->scrubTime = frame / static_cast<float>(fps_);
		Object3d* t = context_->GetTargetObject();
		if (t && t->GetModel() && t->GetModel()->GetMotionSystem()) {
			auto* ms = t->GetModel()->GetMotionSystem();
			ms->Stop();
			context_->isPlaying = false;
			ms->SetAnimationTime(context_->scrubTime);
		}
		});

	// KF削除時のコールバック設定（1回だけ登録）
	dopeSheet_.SetDeleteKeyCallback([this](int trackIdx, int keyIdx) {
		if (trackIdx >= static_cast<int>(tracks_.size())) return;
		if (!context_->currentMotion) return;
		if (keyIdx >= static_cast<int>(tracks_[trackIdx].keys.size())) return;

		// ★修正点1: 選択中ボーンではなく、クリックされたトラックのボーン名を取得する
		if (trackIdx >= static_cast<int>(trackBoneMap_.size())) return;
		std::string targetBoneName = trackBoneMap_[trackIdx].first;

		int frame = tracks_[trackIdx].keys[keyIdx].frame;
		float t = frame / static_cast<float>(fps_);

		// ★修正点2: コンテキストに登録された「削除ロジック」を呼び出す
		if (context_->DeleteKeyframe) {
			context_->DeleteKeyframe(targetBoneName, t);

			// データを変更したのでドープシートの再構築フラグを立てる
			tracksDirty_ = true;
			context_->statusMsg = "KF 削除: " + targetBoneName;
		}
		});
}

void TimelinePanel::DrawImGui()
{
#ifdef USE_IMGUI
	if (!context_->currentMotion) {
		ImGui::TextDisabled("アニメーションを選択するとタイムラインが表示されます");
		return;
	}

	auto& nodeAnims = context_->currentMotion->animation_.nodeAnimations_;
	if (nodeAnims.empty()) {
		ImGui::TextDisabled("アニメーションデータなし");
		return;
	}

	if (ImGui::CollapsingHeader("タイムライン", ImGuiTreeNodeFlags_DefaultOpen))
	{

		if (context_->currentMotion != editingMotion_) {
			tracksDirty_ = true;
			editingMotion_ = context_->currentMotion; // 記憶しておく
		}
		if (context_->requireTimelineRebuild) {
			tracksDirty_ = true;
			context_->requireTimelineRebuild = false;
		}

		// フラグが立っていたらトラック（ドープシートの行とキーフレーム）を作り直す
		if (tracksDirty_) {
			RebuildTracks();
			tracksDirty_ = false;
		}

		float duration = context_->currentMotion->GetDuration();
		const int totalFrames = std::max(1, static_cast<int>(duration * fps_));
		float canvasW = ImGui::GetContentRegionAvail().x;

		ImGui::Text("Time:");
		ImGui::SameLine(0, 4);
		ImGui::SetNextItemWidth(canvasW - 300);

		Object3d* target = context_->GetTargetObject();

		// スライダー操作時に一時停止＆同期
		if (ImGui::SliderFloat("##scrub", &context_->scrubTime, 0.0f, duration, "%.3f s")) {
			if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
				auto* ms = target->GetModel()->GetMotionSystem();
				ms->Stop();
				context_->isPlaying = false;
				ms->SetAnimationTime(context_->scrubTime);
			}
			dopeSheet_.SetSeekFrame(static_cast<int>(context_->scrubTime * fps_));
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("クリック/ドラッグで時刻を移動");

		ImGui::SameLine(0, 8);
		if (ImGui::SmallButton("|<")) {
			context_->scrubTime = 0;
			if (target && target->GetModel() && target->GetModel()->GetMotionSystem()) {
				target->GetModel()->GetMotionSystem()->SetAnimationTime(0.0f);
			}
			dopeSheet_.SetSeekFrame(0);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("先頭に戻る");

		ImGui::SameLine(0, 14);
		ImGui::TextColored(ImVec4(1.0f, 0.59f, 0.2f, 1.0f), (std::string(Icon::ArrowsAlt) + " 位置").c_str());
		ImGui::SameLine(0, 6);
		ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.35f, 1.0f), (std::string(Icon::SyncAlt) + " 回転").c_str());
		ImGui::SameLine(0, 6);
		ImGui::TextColored(ImVec4(0.31f, 0.59f, 1.0f, 1.0f), (std::string(Icon::ExpandArrowsAlt) + " 拡縮").c_str());
		ImGui::SameLine(0, 6);
		ImGui::TextColored(ImVec4(1.0f, 0.94f, 0.2f, 1.0f), (std::string(Icon::CheckCircle) + " 選択中").c_str());

		ImGui::Separator();

		if (tracksDirty_) {
			RebuildTracks();
			tracksDirty_ = false;
		}

		// ドープシート操作時に一時停止＆同期
		dopeSheet_.SetSeekCallback([this](int frame) {
			context_->scrubTime = frame / static_cast<float>(fps_);
			Object3d* t = context_->GetTargetObject();
			if (t && t->GetModel()) {
				auto* ms = t->GetModel()->GetMotionSystem();
				if (ms) {
					ms->Stop();
					context_->isPlaying = false;
					ms->SetAnimationTime(context_->scrubTime);
				}
			}
			});

		dopeSheet_.SetDeleteKeyCallback([this](int trackIdx, int keyIdx) {
			if (trackIdx >= static_cast<int>(tracks_.size())) return;
			if (!context_->currentMotion) return;
			if (keyIdx >= static_cast<int>(tracks_[trackIdx].keys.size())) return;
			int frame = tracks_[trackIdx].keys[keyIdx].frame;
			float t = frame / static_cast<float>(fps_);
			context_->DeleteKeyframe(context_->selKF.boneName, t);
			tracksDirty_ = true;
			context_->statusMsg = "KF 削除";
			});

		bool changed = dopeSheet_.Draw("MotionTimeline", tracks_, totalFrames, fps_);

		if (changed) {
			ApplyTracksToMotion();
			tracksDirty_ = true;
			context_->statusMsg = "KF 移動完了";
		}

		{
			int seekFrame = dopeSheet_.GetSeekFrame();
			float newTime = seekFrame / static_cast<float>(fps_);
			if (std::abs(newTime - context_->scrubTime) > 1e-4f) {
				context_->scrubTime = newTime;
			}
		}

		if (draggingKF_ && !ImGui::IsMouseDown(0)) {
			draggingKF_ = false;
			context_->statusMsg = "KF 移動了";
		}
	}
#endif
}

void TimelinePanel::RebuildTracks()
{
	tracks_.clear();
	trackBoneMap_.clear();
	if (!context_->currentMotion) return;

	static const DopeSheet::Color colT = { 1.0f, 0.59f, 0.2f, 1.0f };
	static const DopeSheet::Color colR = { 0.35f, 0.78f, 0.35f, 1.0f };
	static const DopeSheet::Color colS = { 0.31f, 0.59f, 1.0f, 1.0f };

	auto& nodeAnims = context_->currentMotion->animation_.nodeAnimations_;

	std::vector<std::string> boneNames;
	boneNames.reserve(nodeAnims.size());
	for (const auto& [name, _] : nodeAnims) boneNames.push_back(name);
	std::sort(boneNames.begin(), boneNames.end());

	for (const auto& boneName : boneNames)
	{
		const auto& na = nodeAnims.at(boneName);

		{
			DopeSheet::DopeTrack header;
			header.label = boneName;
			header.isGroupHeader = true;
			header.groupExpanded = (context_->selBone == boneName || context_->selKF.boneName == boneName
				? true : header.groupExpanded);
			tracks_.push_back(header);
			trackBoneMap_.push_back({ boneName, -1 });
		}

		{
			DopeSheet::DopeTrack t;
			t.label = "  T";
			t.color = colT;
			t.groupDepth = 1;
			for (const auto& kf : na.translate.keyframes)
				t.keys.emplace_back(static_cast<int>(kf.time * fps_ + 0.5f), kf.value.x, 0);
			tracks_.push_back(t);
			trackBoneMap_.push_back({ boneName, 0 });
		}
		{
			DopeSheet::DopeTrack r;
			r.label = "  R";
			r.color = colR;
			r.groupDepth = 1;
			for (const auto& kf : na.rotate.keyframes)
				r.keys.emplace_back(static_cast<int>(kf.time * fps_ + 0.5f), kf.value.w, 1);
			tracks_.push_back(r);
			trackBoneMap_.push_back({ boneName, 1 });
		}
		{
			DopeSheet::DopeTrack s;
			s.label = "  S";
			s.color = colS;
			s.groupDepth = 1;
			for (const auto& kf : na.scale.keyframes)
				s.keys.emplace_back(static_cast<int>(kf.time * fps_ + 0.5f), kf.value.x, 2);
			tracks_.push_back(s);
			trackBoneMap_.push_back({ boneName, 2 });
		}
	}

	// ── 概要トラックを先頭に挿入 ──
	BuildSummaryTrack();
}

void TimelinePanel::ApplyTracksToMotion()
{
	if (!context_->currentMotion) return;

	for (int ti = 0; ti < static_cast<int>(tracks_.size()); ++ti)
	{
		if (ti >= static_cast<int>(trackBoneMap_.size())) break;
		const auto& [boneName, channel] = trackBoneMap_[ti];

		// channel == -2: 概要トラック（スキップ）
		// channel == -1: グループヘッダー（スキップ）
		if (channel < 0) continue;

		if (!context_->currentMotion->animation_.nodeAnimations_.count(boneName)) continue;

		auto& na = context_->currentMotion->animation_.nodeAnimations_[boneName];
		const auto& keys = tracks_[ti].keys;

		auto applyTimes = [&](auto& keyframes) {
			if (keys.size() != keyframes.size()) return;
			for (int ki = 0; ki < static_cast<int>(keys.size()); ++ki)
				keyframes[ki].time = keys[ki].frame / static_cast<float>(fps_);
			std::sort(keyframes.begin(), keyframes.end(),
				[](const auto& a, const auto& b) { return a.time < b.time; });
			};

		switch (channel) {
		case 0: applyTimes(na.translate.keyframes); break;
		case 1: applyTimes(na.rotate.keyframes);    break;
		case 2: applyTimes(na.scale.keyframes);     break;
		}
	}
}

// ============================================================
// 概要トラック生成
//   全ボーン・全チャンネルのKF時刻を収集して重複排除し、
//   1行のサマリートラックとして先頭に挿入する
// ============================================================
void TimelinePanel::BuildSummaryTrack()
{
	if (!context_->currentMotion) return;

	// 全チャンネルのフレーム番号を収集（重複排除）
	std::set<int> allFrames;
	for (const auto& [name, na] : context_->currentMotion->animation_.nodeAnimations_) {
		for (const auto& kf : na.translate.keyframes)
			allFrames.insert(static_cast<int>(kf.time * fps_ + 0.5f));
		for (const auto& kf : na.rotate.keyframes)
			allFrames.insert(static_cast<int>(kf.time * fps_ + 0.5f));
		for (const auto& kf : na.scale.keyframes)
			allFrames.insert(static_cast<int>(kf.time * fps_ + 0.5f));
	}

	DopeSheet::DopeTrack summary;
	summary.label = "概要";
	summary.isSummary = true;
	summary.color = { 1.0f, 0.82f, 0.24f, 1.0f };
	for (int frame : allFrames)
		summary.keys.emplace_back(frame, 0.0f, -1);

	// tracks_ / trackBoneMap_ の先頭に挿入
	tracks_.insert(tracks_.begin(), summary);
	trackBoneMap_.insert(trackBoneMap_.begin(), { "__summary__", -2 });
}

// ============================================================
// 概要トラックKF一括移動ハンドラ
//   指定フレームに存在する全ボーン・全チャンネルのKFを
//   delta フレーム分ずらし、CommandHistory に登録する
// ============================================================
void TimelinePanel::OnSummaryKeyMoved(int fromFrame, int delta)
{
	if (!context_->currentMotion || delta == 0) return;

	// Undo用スナップショット
	auto oldAnims = context_->currentMotion->animation_.nodeAnimations_;

	// 全ボーン・全チャンネルを走査して fromFrame のKFを移動
	for (auto& [boneName, na] : context_->currentMotion->animation_.nodeAnimations_) {

		auto moveInTrack = [&](auto& keyframes) {
			for (auto& kf : keyframes) {
				int kfFrame = static_cast<int>(kf.time * fps_ + 0.5f);
				if (kfFrame == fromFrame) {
					float newTime = (fromFrame + delta) / static_cast<float>(fps_);
					kf.time = std::max(0.0f, newTime);
				}
			}
			// 移動後に時刻順ソートを保証
			std::sort(keyframes.begin(), keyframes.end(),
				[](const auto& a, const auto& b) { return a.time < b.time; });
			};

		moveInTrack(na.translate.keyframes);
		moveInTrack(na.rotate.keyframes);
		moveInTrack(na.scale.keyframes);
	}

	auto newAnims = context_->currentMotion->animation_.nodeAnimations_;

	// CommandHistory にUndoable コマンドとして登録
	context_->history.Execute(MakeLambdaCommand(
		"概要KF移動: " + std::to_string(fromFrame) + " → " + std::to_string(fromFrame + delta),
		[this, newAnims]() {
			if (context_->currentMotion)
				context_->currentMotion->animation_.nodeAnimations_ = newAnims;
			tracksDirty_ = true;
		},
		[this, oldAnims]() {
			if (context_->currentMotion)
				context_->currentMotion->animation_.nodeAnimations_ = oldAnims;
			tracksDirty_ = true;
		}
	));

	tracksDirty_ = true;
	context_->statusMsg = "概要: frame " + std::to_string(fromFrame)
		+ (delta > 0 ? " +" : " ") + std::to_string(delta) + " 移動";
}