#pragma once
#include "IMotionEditorPanel.h"
class BoneListPanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override;
	void Update() override;
	void DrawImGui() override;

private:
	MotionEditorContext* context_ = nullptr;
};

