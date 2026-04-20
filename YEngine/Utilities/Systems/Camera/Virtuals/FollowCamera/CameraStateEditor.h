#pragma once
#include "CameraState.h"
#include <memory>
#include <string>

class FollowCamera;

// ============================================================
// カメラ��テートを編集・管理するための統合エディタ
// ============================================================
class CameraStateEditor {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	static CameraStateEditor* GetInstance();

	void DrawEditorWindow();
	void SetEditingState(std::unique_ptr<CameraState> state);
	void SetCamera(FollowCamera* camera) { camera_ = camera; }

	CameraState* GetEditingState() const { return editingState_.get(); }

	// ============================================================
	// プレビュー操作
	// ============================================================
	void StartPreview();
	void StopPreview();
	void NotifyPreviewFinished() { isPreviewMode_ = false; }

private:
	// ============================================================
	// 内部処理・UI描画関数
	// ============================================================
	CameraStateEditor() = default;
	~CameraStateEditor() = default;
	CameraStateEditor(const CameraStateEditor&) = delete;
	CameraStateEditor& operator=(const CameraStateEditor&) = delete;

	void DrawStateCreationUI();
	void DrawStateEditorUI();
	void DrawPresetManagerUI();
	void DrawPreviewControlUI();

private:
	std::unique_ptr<CameraState> editingState_;
	FollowCamera* camera_ = nullptr;

	int selectedStateType_ = 0;
	char newPresetName_[64] = "";
	bool isPreviewMode_ = false;
};