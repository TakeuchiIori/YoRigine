#pragma once
// Engine
#include <Editor/Command/CommandHistory.h>
// App
#include "KeyFrame.h"

// ============================================================
// 前方宣言
// ============================================================
class Motion;
class Camera;
class Object3d;

// ============================================================
// モーションエディタの共有コンテキスト
// ============================================================
struct MotionEditorContext
{
	Motion* currentMotion = nullptr;
	Camera* camera = nullptr;
	int targetObjectId = -1;

	// ============================================================
	// モデル・ファイル・アニメーション状態
	// ============================================================
	std::string loadFileName = "";
	std::string selectedAnimKey = "";

	// 再生状態
	float scrubTime = 0.0f;
	bool isPlaying = true;
	bool isLoop = true;
	float timelineZoom = 80.0f;

	// 選択状態
	std::string selBone = "";
	SelectedKF selKF;

	// 編集用バッファ
	float editT[3] = { 0, 0, 0 };
	float editR[3] = { 0, 0, 0 };
	float editS[3] = { 1, 1, 1 };

	// UIフラグとレイアウト
	bool isDrawBone = false;
	bool showSavePopup = false;
	std::string statusMsg = "Ready";
	bool requireTimelineRebuild = false;

	float timelineH = 200.0f;
	float bonePanelW = 250.0f;

	// ============================================================
	// コールバック関数
	// ============================================================
	std::function<void()> SyncJointToBuffer;
	std::function<void()> SyncBufferToJoint;
	std::function<void(const std::string&, float)> AddKeyframe;
	std::function<void(const std::string&, float)> DeleteKeyframe;

	// ============================================================
	// 履歴管理
	// ============================================================
	CommandHistory history;

	// ============================================================
	// ヘルパー関数
	// ============================================================
	Object3d* GetTargetObject() const;
};