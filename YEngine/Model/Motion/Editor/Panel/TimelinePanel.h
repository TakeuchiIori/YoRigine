#pragma once
#include "IMotionEditorPanel.h"
#include "../MotionEditorContext.h"
#include "Debugger/DopeSheet/DopeSheetEditor.h"
#include <vector>

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
};