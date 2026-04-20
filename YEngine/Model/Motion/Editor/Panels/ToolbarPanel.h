#pragma once
#include "IMotionEditorPanel.h"
#include "../MotionEditorContext.h"

/// <summary>
/// 再生制御やアニメーション選択を行うツールバー
/// </summary>
class ToolbarPanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override { context_ = context; }
	void Update() override {}
	void DrawImGui() override;

private:
	MotionEditorContext* context_ = nullptr;
};