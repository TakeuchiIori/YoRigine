#include "DopeSheetEditor.h"
#include <algorithm>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

//=============================================================================
// メイン描画
//=============================================================================
bool DopeSheetEditor::Draw(
	const char* id,
	std::vector<DopeTrack>& tracks,
	int                     totalFrames,
	int                     fps,
	float                   height)
{
#ifndef USE_IMGUI
	return false;
#else
	bool anyChanged = false;
	totalFrames = std::max(1, totalFrames);

	//-------------------------------------------------------------------------
	// 表示するトラック行数を数える（非表示・グループ折りたたみを除外）
	//-------------------------------------------------------------------------
	int visibleRows = 0;
	{
		bool inCollapsedGroup = false;
		for (const auto& t : tracks)
		{
			if (!t.visible) continue;
			if (t.isGroupHeader)
			{
				inCollapsedGroup = !t.groupExpanded;
				visibleRows++;
				continue;
			}
			if (!inCollapsedGroup) visibleRows++;
		}
	}

	const float timelineW = kLabelW + totalFrames * zoomX_ + 20.0f;
	const float timelineH = (height > 0.0f)
		? height
		: kRulerH + kRowH * visibleRows + 4.0f;

	//-------------------------------------------------------------------------
	// ズームスライダー（ツールバー）
	//-------------------------------------------------------------------------
	ImGui::PushID(id);
	ImGui::SetNextItemWidth(120);
	ImGui::SliderFloat("ズーム", &zoomX_, 2.0f, 60.0f, "%.0fpx/f");
	ImGui::SameLine();
	ImGui::Text("| シーク: %d f  (%.2f s)", seekFrame_, seekFrame_ / (float)fps);
	ImGui::SameLine();
	if (ImGui::SmallButton("リセット")) ResetView();

	//-------------------------------------------------------------------------
	// スクロール可能な子ウィンドウ
	//-------------------------------------------------------------------------
	ImGui::BeginChild(
		(std::string("##DopeScroll_") + id).c_str(),
		{ 0, timelineH + 4 },
		false,
		ImGuiWindowFlags_HorizontalScrollbar);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	// スクロール幅確保用ダミー
	ImGui::Dummy({ timelineW, timelineH });

	//-------------------------------------------------------------------------
	// 背景縞模様
	//-------------------------------------------------------------------------
	{
		bool inCollapsed = false;
		int row = 0;
		for (const auto& t : tracks)
		{
			if (!t.visible) continue;
			if (t.isGroupHeader)
			{
				inCollapsed = !t.groupExpanded;
				// グループヘッダー背景
				float y = origin.y + kRulerH + row * kRowH;
				dl->AddRectFilled(
					{ origin.x, y },
					{ origin.x + timelineW, y + kHeaderH },
					IM_COL32(35, 55, 75, 255));
				row++;
				continue;
			}
			if (inCollapsed) continue;

			float y = origin.y + kRulerH + row * kRowH;
			ImU32 bg = (row % 2 == 0)
				? IM_COL32(46, 46, 46, 255)
				: IM_COL32(38, 38, 38, 255);
			dl->AddRectFilled(
				{ origin.x, y },
				{ origin.x + timelineW, y + kRowH },
				bg);
			row++;
		}
	}

	//-------------------------------------------------------------------------
	// ルーラー
	//-------------------------------------------------------------------------
	DrawRuler(dl, origin, zoomX_, totalFrames, fps);

	//-------------------------------------------------------------------------
	// 各トラック
	//-------------------------------------------------------------------------
	{
		bool inCollapsed = false;
		int row = 0;
		for (int ti = 0; ti < static_cast<int>(tracks.size()); ++ti)
		{
			DopeTrack& track = tracks[ti];
			if (!track.visible) continue;

			float y = origin.y + kRulerH + row * kRowH;

			if (track.isGroupHeader)
			{
				// グループヘッダー行
				inCollapsed = !track.groupExpanded;

				// 折りたたみ三角
				ImVec2 triPos = { origin.x + 4 + track.groupDepth * 12.0f, y + kHeaderH * 0.5f };
				const char* arrow = track.groupExpanded ? "▼" : "▶";
				dl->AddText(triPos, IM_COL32(200, 200, 200, 255), arrow);

				// ラベル
				dl->AddText(
					{ triPos.x + 16, y + 4 },
					IM_COL32(230, 230, 230, 255),
					track.label.c_str());

				// クリックで折りたたみトグル
				ImGui::SetCursorScreenPos({ origin.x, y });
				ImGui::InvisibleButton(
					(std::string("##grp_") + std::to_string(ti)).c_str(),
					{ kLabelW + totalFrames * zoomX_, kHeaderH });
				if (ImGui::IsItemClicked())
				{
					track.groupExpanded = !track.groupExpanded;
					anyChanged = true;
				}

				row++;
				continue;
			}

			if (inCollapsed) continue;

			// ラベル
			ImVec2 labelPos = { origin.x + 6 + track.groupDepth * 12.0f, y + 4 };
			dl->AddText(labelPos, IM_COL32(180, 180, 180, 255), track.label.c_str());

			// トラック行を描画
			ImVec2 rowMin = { origin.x + kLabelW, y };
			if (DrawTrackRow(dl, rowMin, zoomX_, totalFrames, track, ti))
				anyChanged = true;

			row++;
		}
	}

	//-------------------------------------------------------------------------
	// シークバー
	//-------------------------------------------------------------------------
	DrawSeekBar(dl, origin, timelineH, totalFrames);

	//-------------------------------------------------------------------------
	// ルーラークリックでシーク
	//-------------------------------------------------------------------------
	{
		ImVec2 rMin = { origin.x + kLabelW, origin.y };
		ImVec2 rMax = { origin.x + timelineW, origin.y + kRulerH };
		if (ImGui::IsMouseHoveringRect(rMin, rMax) && ImGui::IsMouseDown(0))
		{
			int newFrame = std::clamp(
				static_cast<int>((ImGui::GetMousePos().x - rMin.x) / zoomX_),
				0, totalFrames);
			if (newFrame != seekFrame_)
			{
				seekFrame_ = newFrame;
				if (onSeek_) onSeek_(seekFrame_);
			}
		}
	}

	ImGui::EndChild();

	//-------------------------------------------------------------------------
	// 追加ポップアップ
	//-------------------------------------------------------------------------
	DrawAddKeyPopup(tracks);

	ImGui::PopID();
	return anyChanged;
#endif
}

//=============================================================================
// ルーラー描画
//=============================================================================
void DopeSheetEditor::DrawRuler(ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames, int fps)
{
#ifdef USE_IMGUI
	const float totalW = kLabelW + totalFrames * cellW + 20.0f;

	// 背景
	dl->AddRectFilled(
		{ origin.x, origin.y },
		{ origin.x + totalW, origin.y + kRulerH },
		IM_COL32(30, 30, 30, 255));

	// フレーム目盛り（密度に応じて間引く）
	const int step = std::max(1, static_cast<int>(30.0f / cellW));
	for (int f = 0; f <= totalFrames; ++f)
	{
		float x = origin.x + kLabelW + f * cellW;
		bool isMajor = (f % fps == 0); // 1秒ごとに強調

		// グリッド縦線
		ImU32 gridCol = isMajor ? IM_COL32(100, 100, 100, 200)
			: (f % 5 == 0) ? IM_COL32(70, 70, 70, 150)
			: IM_COL32(50, 50, 50, 100);
		dl->AddLine(
			{ x, origin.y + kRulerH },
			{ x, origin.y + kRulerH + kRowH * 64 }, // 十分に長く
			gridCol);

		// 目盛り線
		float tickH = isMajor ? kRulerH * 0.6f : kRulerH * 0.35f;
		dl->AddLine(
			{ x, origin.y + kRulerH - tickH },
			{ x, origin.y + kRulerH },
			IM_COL32(180, 180, 180, 200));

		// フレーム番号テキスト（間引き）
		if (f % step == 0)
		{
			char buf[12];
			if (isMajor)
				std::snprintf(buf, sizeof(buf), "%ds", f / fps);
			else
				std::snprintf(buf, sizeof(buf), "%d", f);
			dl->AddText({ x + 2, origin.y + 2 }, IM_COL32(180, 180, 180, 255), buf);
		}
	}

	// ラベル列の区切り
	dl->AddLine(
		{ origin.x + kLabelW, origin.y },
		{ origin.x + kLabelW, origin.y + kRulerH },
		IM_COL32(80, 80, 80, 255));

	// ルーラーオーバーレイコールバック
	if (onRulerOverlay_)
		onRulerOverlay_(dl, origin, cellW, totalFrames);
#endif
}

//=============================================================================
// トラック行描画（キーフレームのインタラクション込み）
//=============================================================================
bool DopeSheetEditor::DrawTrackRow(
	ImDrawList* dl,
	ImVec2 rowMin,
	float cellW,
	int totalFrames,
	DopeTrack& track,
	int trackIdx)
{
#ifndef USE_IMGUI
	return false;
#else
	bool changed = false;
	const float halfH = kRowH * 0.5f;
	const float radius = std::min(halfH * 0.55f, cellW * 0.45f);

	//-------------------------------------------------------------------------
	// ドラッグ処理（マウスが離れた）
	//-------------------------------------------------------------------------
	if (drag_.active && drag_.trackIdx == trackIdx)
	{
		if (!ImGui::IsMouseDown(0))
		{
			// フレーム順ソート
			std::sort(track.keys.begin(), track.keys.end(),
				[](const DopeKey& a, const DopeKey& b) { return a.frame < b.frame; });
			drag_.active = false;
			changed = true;
		}
	}

	//-------------------------------------------------------------------------
	// 各キーフレームを描画
	//-------------------------------------------------------------------------
	int removeIdx = -1;
	for (int ki = 0; ki < static_cast<int>(track.keys.size()); ++ki)
	{
		DopeKey& key = track.keys[ki];
		float cx = rowMin.x + key.frame * cellW;
		float cy = rowMin.y + halfH;

		ImU32 fillCol = GetKeyColor(track, key.subType, false);
		ImU32 outlineCol = IM_COL32(255, 255, 255, 60);

		bool wasChanged = DrawKey(dl, { cx, cy }, radius, fillCol, outlineCol,
			key, trackIdx, ki, cellW, totalFrames);
		if (wasChanged && !track.readOnly) changed = true;

		// Deleteキーで選択中のキーを削除
		if (key.selected && ImGui::IsKeyPressed(ImGuiKey_Delete) && !track.readOnly)
		{
			removeIdx = ki;
		}
	}

	if (removeIdx >= 0)
	{
		if (onDeleteKey_) onDeleteKey_(trackIdx, removeIdx);
		else              track.keys.erase(track.keys.begin() + removeIdx);
		changed = true;
	}

	//-------------------------------------------------------------------------
	// 右クリックで追加ポップアップ（readOnlyでなければ）
	//-------------------------------------------------------------------------
	if (!track.readOnly)
	{
		ImVec2 trackMax = { rowMin.x + totalFrames * cellW, rowMin.y + kRowH };
		if (ImGui::IsMouseHoveringRect(rowMin, trackMax) && ImGui::IsMouseClicked(1))
		{
			float mx = ImGui::GetMousePos().x - rowMin.x;
			pendingFrame_ = std::clamp(static_cast<int>(mx / cellW), 0, totalFrames);
			pendingValue_ = 0.0f;
			pendingTrackIdx_ = trackIdx;
			showAddPopup_ = true;
			ImGui::OpenPopup("##DopeAddKey");
		}
	}

	return changed;
#endif
}

//=============================================================================
// ひし形キー描画 + インタラクション
//=============================================================================
bool DopeSheetEditor::DrawKey(
	ImDrawList* dl,
	ImVec2 center,
	float radius,
	ImU32 fillCol,
	ImU32 outlineCol,
	DopeKey& key,
	int trackIdx,
	int keyIdx,
	float cellW,
	int totalFrames)
{
#ifndef USE_IMGUI
	return false;
#else
	bool changed = false;
	const float cx = center.x, cy = center.y;

	// 選択中は白いアウトライン
	if (key.selected) outlineCol = IM_COL32(255, 255, 255, 200);

	// ◆ ひし形
	dl->AddQuadFilled(
		{ cx,        cy - radius },
		{ cx + radius, cy },
		{ cx,        cy + radius },
		{ cx - radius, cy },
		fillCol);
	dl->AddQuad(
		{ cx,        cy - radius },
		{ cx + radius, cy },
		{ cx,        cy + radius },
		{ cx - radius, cy },
		outlineCol, 1.2f);

	// ヒットエリア（少し広め）
	ImVec2 hitMin = { cx - radius - 4, cy - radius - 4 };
	ImVec2 hitMax = { cx + radius + 4, cy + radius + 4 };
	bool hovered = ImGui::IsMouseHoveringRect(hitMin, hitMax);

	if (hovered)
	{
		// ホバー強調
		dl->AddQuad(
			{ cx,          cy - radius - 2 },
			{ cx + radius + 2, cy },
			{ cx,          cy + radius + 2 },
			{ cx - radius - 2, cy },
			IM_COL32(255, 255, 255, 120), 1.5f);

		// ツールチップ
		ImGui::BeginTooltip();
		ImGui::Text("frame=%d  value=%.3f  sub=%d", key.frame, key.value, key.subType);
		ImGui::EndTooltip();

		// ドラッグ開始
		if (ImGui::IsMouseClicked(0) && !drag_.active)
		{
			drag_.active = true;
			drag_.trackIdx = trackIdx;
			drag_.keyIdx = keyIdx;
			drag_.startFrame = key.frame;
			drag_.startMouseX = ImGui::GetMousePos().x;
			key.selected = true;
		}
	}

	// ドラッグ中の移動
	if (drag_.active && drag_.trackIdx == trackIdx && drag_.keyIdx == keyIdx)
	{
		if (ImGui::IsMouseDown(0))
		{
			float delta = ImGui::GetMousePos().x - drag_.startMouseX;
			int newFrame = std::clamp(
				drag_.startFrame + static_cast<int>(delta / cellW),
				0, totalFrames);
			if (newFrame != key.frame) { key.frame = newFrame; changed = true; }
		}
	}

	return changed;
#endif
}

//=============================================================================
// シークバー描画
//=============================================================================
void DopeSheetEditor::DrawSeekBar(ImDrawList* dl, ImVec2 origin, float timelineH, int totalFrames)
{
#ifdef USE_IMGUI
	if (seekFrame_ < 0 || seekFrame_ > totalFrames) return;
	float sx = origin.x + kLabelW + seekFrame_ * zoomX_;
	// 三角ヘッド
	dl->AddTriangleFilled(
		{ sx - 5, origin.y },
		{ sx + 5, origin.y },
		{ sx,     origin.y + 10 },
		IM_COL32(255, 220, 50, 230));
	// 縦線
	dl->AddLine(
		{ sx, origin.y + 10 },
		{ sx, origin.y + timelineH },
		IM_COL32(255, 220, 50, 180), 1.5f);
#endif
}

//=============================================================================
// キー追加ポップアップ
//=============================================================================
void DopeSheetEditor::DrawAddKeyPopup(std::vector<DopeTrack>& tracks)
{
#ifdef USE_IMGUI
	if (ImGui::BeginPopup("##DopeAddKey"))
	{
		ImGui::Text("キー追加  [%s] frame=%d",
			(pendingTrackIdx_ >= 0 && pendingTrackIdx_ < static_cast<int>(tracks.size()))
			? tracks[pendingTrackIdx_].label.c_str() : "?",
			pendingFrame_);
		ImGui::Separator();

		ImGui::SetNextItemWidth(100);
		ImGui::InputFloat("値", &pendingValue_, 0.1f);

		ImGui::Separator();
		if (ImGui::Button("追加"))
		{
			if (pendingTrackIdx_ >= 0 && pendingTrackIdx_ < static_cast<int>(tracks.size()))
			{
				if (onAddKey_)
				{
					onAddKey_(pendingTrackIdx_, pendingFrame_, pendingValue_);
				} else
				{
					auto& t = tracks[pendingTrackIdx_];
					if (!t.readOnly)
					{
						t.keys.emplace_back(pendingFrame_, pendingValue_);
						std::sort(t.keys.begin(), t.keys.end(),
							[](const DopeKey& a, const DopeKey& b) { return a.frame < b.frame; });
					}
				}
				pendingValue_ = 0.0f;
				showAddPopup_ = false;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル"))
		{
			showAddPopup_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
#endif
}

//=============================================================================
// キー色取得
//=============================================================================
ImU32 DopeSheetEditor::GetKeyColor(const DopeTrack& track, int subType, bool hovered) const
{
#ifdef USE_IMGUI
	ImVec4 col;
	if (!track.subColors.empty() && subType >= 0 && subType < static_cast<int>(track.subColors.size()))
		col = track.subColors[subType];
	else
		col = track.color;

	if (hovered)
	{
		col.x = std::min(col.x + 0.3f, 1.0f);
		col.y = std::min(col.y + 0.3f, 1.0f);
		col.z = std::min(col.z + 0.3f, 1.0f);
	}
	return ImGui::ColorConvertFloat4ToU32(col);
#else
	return 0;
#endif
}