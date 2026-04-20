#pragma once
#include "IMotionEditorPanel.h"
#include "../MotionEditorContext.h"
#include "Quaternion.h"

/// <summary>
/// 選択中のボーンやキーフレームのパラメータを編集するパネル
/// </summary>
class PropertyPanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override { context_ = context; }
	void DrawImGui() override;

private:
	MotionEditorContext* context_ = nullptr;
	bool draggingBone_ = false;

	// ドラッグ中の元データ保持用
	QuaternionTransform boneSnap_;
	Vector3 kfSnapT_;
	Quaternion kfSnapR_;
	Vector3 kfSnapS_;
};