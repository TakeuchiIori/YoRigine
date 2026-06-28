#pragma once
#include "IMotionEditorPanel.h"

class StatusBarPanel : public IMotionEditorPanel {
public:
	void Initialize(MotionEditorContext* context) override { context_ = context; }
	void DrawImGui() override;
private:
	MotionEditorContext* context_ = nullptr;
};
