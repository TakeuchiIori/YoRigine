#pragma once
#include "IMotionEditorPanel.h"
#include <Model/Skeleton/Joint.h>

#include <vector>
class BoneListPanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override;
	void Update() override;
	void DrawImGui() override;

private:
#ifdef USE_IMGUI
	void DrawBoneNode(const std::vector<Joint>& joints, int jointIndex, const std::vector<std::vector<int>>& childrenMap);
#endif
private:
	MotionEditorContext* context_ = nullptr;
};

