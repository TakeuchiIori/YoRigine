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
	// ============================================================
	// 公開変数
	// ============================================================
	Motion* currentMotion = nullptr;
	Camera* camera = nullptr;
	int targetObjectId = -1;

	// 再生状態
	float scrubTime = 0.0f;
	bool isPlaying = true;
	bool isLoop = true;
	float timelineZoom = 80.0f;

	// 選択状態
	std::string selBone = "";
	SelectedKF selKF;

	// 編集用バッファ（プロパティパネルとギズモ操作で共有）
	float editT[3] = { 0, 0, 0 };
	float editR[3] = { 0, 0, 0 };
	float editS[3] = { 1, 1, 1 };

	// その他UIフラグ
	bool isDrawBone = false;
	bool showSavePopup = false; // ツールバーとメニューで共有
	std::string statusMsg = "Ready";
	bool requireTimelineRebuild = false;

	// 履歴管理
	CommandHistory history;

	// ============================================================
	// 共通関数（デリゲート / ヘルパー）
	// 各パネルがMotionEditor本体の機能を使えるようにする
	// ============================================================
	std::function<void()> SyncJointToBuffer;  // Jointの現在値 -> 編集バッファ
	std::function<void()> SyncBufferToJoint;  // 編集バッファ -> Joint
	std::function<void(const std::string&, float time)> AddKeyframe; // KF挿入処理
	std::function<void(const std::string&, float)> DeleteKeyframe; // KF削除処理

	// 対象オブジェクト取得用ヘルパー
	Object3d* GetTargetObject() const;
};