#pragma once
#include "IMotionEditorPanel.h"
#include "../MotionEditorContext.h"

/// <summary>
/// 再生制御やアニメーション選択、ゲーム向けモーションユーティリティを行うツールバー
/// </summary>
class ToolbarPanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override { context_ = context; }
	void Update() override {}
	void DrawImGui() override;

private:
	// ミラーモーションの生成
	void GenerateMirrorMotion();
	
	// トリム範囲を切り出して新しいモーションを生成
	void TrimMotion();

	// ピンポン再生用のモーションを生成
	void GeneratePingPongMotion();

	// 指定フレーム範囲に移動オフセットをTranslateキーとして焼き込む
	void BakeTranslateOffsetRange();

	MotionEditorContext* context_ = nullptr;

	int offsetStartFrame_ = 2;
	int offsetEndFrame_ = 10;
	float offsetTranslate_[3] = { 0.0f, 1.0f, 0.0f };
};
