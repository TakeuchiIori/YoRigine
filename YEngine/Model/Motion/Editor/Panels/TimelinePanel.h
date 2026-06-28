#pragma once
#include "IMotionEditorPanel.h"
#include "../MotionEditorContext.h"
#include "Debugger/DopeSheet/DopeSheetEditor.h"
#include <vector>
#include <Motion/Core/Motion.h>

/// <summary>
/// アニメーションのタイムライン（ドープシート）を描画・管理するパネル
/// </summary>
class TimelinePanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override;
	void DrawImGui() override;

	// DopeSheetの再構築要求フラグ（外部からアニメーションが切り替わった時など）
	void SetDirty() { tracksDirty_ = true; }

private:
	void RebuildTracks();
	void ApplyTracksToMotion();

	void BuildSummaryTrack();           // 全KFを集約した概要トラックを生成
	void OnSummaryKeyMoved(int fromFrame, int delta); // 概要KF一括移動ハンドラ

	void RegisterMoveCommand();

	void RegisterSummaryMoveCommand();

private:
	MotionEditorContext* context_ = nullptr;

	// パネル内に完全に隠蔽されたタイムライン専用の変数
	DopeSheet::DopeSheetEditor dopeSheet_;
	std::vector<DopeSheet::DopeTrack> tracks_;
	std::vector<std::pair<std::string, int>> trackBoneMap_;

	bool tracksDirty_ = false;
	int fps_ = 60;
	float timelineH_ = 240.0f;

	// KF ドラッグ移動
	bool  draggingKF_ = false;

	// 概要KFドラッグ用
	bool summaryMovedThisFrame_ = false;
	int  prevSummaryFromFrame_ = -1;
	std::map<std::string, Motion::NodeAnimation> summaryDragSnapshot_;

	// ドラッグ判定用
	// 前フレームのマウス状態を保持
	bool wasMouseDown_ = false;
	// マウスリリース時にApplyTracksを走らせる
	bool pendingApply_ = false;
	bool pendingSummaryApply_ = false; // 概要KF移動の確定待ちフラグ
	std::map<std::string, Motion::NodeAnimation> preApplySnapshot_;
	Motion* editingMotion_ = nullptr; // ドープシートで現在編集しているモーション（変更検知用）
};