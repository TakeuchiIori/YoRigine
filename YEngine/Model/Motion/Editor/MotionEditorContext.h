#pragma once
// Engine
#include <Editor/Command/CommandHistory.h>
// App
#include "KeyFrame.h"
#include "Quaternion.h"
#include "Vector3.h"

// ============================================================
// 前方宣言
// ============================================================
class Motion;
class Camera;
class Object3d;

// ============================================================
// キーフレームクリップボード
// ============================================================
struct KeyframeClipboard
{
	bool hasData = false;
	Vector3 translate = { 0, 0, 0 };
	Quaternion rotate = { 0, 0, 0, 1 };
	Vector3 scale = { 1, 1, 1 };
	// コピー元情報（表示用）
	std::string sourceBone = "";
	float sourceTime = 0.0f;
};

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
	float lastAppliedScrubTime = -1.0f;
	bool isPlaying = true;
	bool isLoop = true;
	bool isReverse = false;
	bool isPingPong = false;
	bool pingPongForward = true;
	float playbackSpeed = 1.0f;
	float timelineZoom = 80.0f;

	// トリム範囲 (0 = 無効)
	float trimStart = 0.0f;
	float trimEnd = 0.0f;
	bool useTrim = false;

	// 選択状態
	std::string selBone = "";
	SelectedKF selKF;

	// 編集用バッファ
	float editT[3] = { 0, 0, 0 };
	float editR[3] = { 0, 0, 0 };
	float editS[3] = { 1, 1, 1 };
	float translateDisplayScale = 100.0f;

	// キーフレームクリップボード
	KeyframeClipboard clipboard;

	// UIフラグとレイアウト
	bool isDrawBone = false;
	bool showSavePopup = false;
	bool showSourceAnimationPopup = false;
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
