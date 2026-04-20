#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>

// Engine
#include "Object3d/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include "Editor/Command/CommandHistory.h"
#include <Graphics/Drawer/LineManager/Line.h>
#include "Object3D/ObjectManager.h" 

// App
#include "MotionEditorContext.h"
#include "Panels/IMotionEditorPanel.h"

#ifdef USE_IMGUI
#include "Debugger/Gizmo/GizmoController.h"
#endif

// Math
#include "Vector3.h"
#include "Quaternion.h"

class Camera;
class Motion;
class Joint;
class Skeleton;

#ifdef USE_IMGUI
class BoneGizmable; // 前方宣言
#endif

// ============================================================
//  ファイルブラウザ状態
// ============================================================
struct FileBrowserState
{
	std::string currentDirectory = "Resources/Models";
	std::string selectedFilePath = "";
	std::vector<std::string> directoryHistory;
	bool isOpen = false;
	std::string filterExtension = "";
};

/// <summary>
/// モーションを編集するエディタ (統括クラス)
/// </summary>
class MotionEditor
{
public:

	///************************* 基本的関数 *************************///
	MotionEditor();
	~MotionEditor();

	void Initialize(Camera* camera);
	void Update();
	void Draw();
	void DrawGizmo();
	void DrawBone();
	void ShowEditor();

	void SetTargetObjectId(int id);
	int GetTargetObjectId() const { return context_.targetObjectId; }
	void SetCamera(Camera* camera) { context_.camera = camera; }
	bool IsDrawBone() const { return context_.isDrawBone; }

	///************************* ギズモ操作用インターフェース *************************///
	Matrix4x4 GetJointWorldMatrix(const std::string& boneName) const;
	void ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat);

	///************************* パネル登録 *************************///
	void RegisterPanel(std::unique_ptr<IMotionEditorPanel> panel);

private:
	///************************* その他描画 *************************///
	void DrawMenuBar();
	void DrawStatusBar();

	///************************* ポップアップ *************************///
	void DrawSaveLoadPopup();
	void DrawFileBrowser(FileBrowserState& state, const char* title);

	///************************* キーフレーム操作 *************************///
	void SavePose(float time);
	void InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr);

	///************************* ボーン操作 *************************///
	void   SetJointTransform(const std::string& bone, const QuaternionTransform& tr);
	Joint* FindJoint(const std::string& name) const;

	///************************* ユーティリティ *************************///
	static std::string AnimDisplayName(const std::string& cacheKey);
	static std::vector<std::string> FetchAnimationNames(const std::string& fullPath);
	std::vector<std::filesystem::directory_entry> GetDirectoryEntries(const std::string& dir, const std::string& ext) const;

	QuaternionTransform BufferToTransform() const;
	void SyncJointToBuffer(const std::string& bone);  // Joint の現在値 → 編集バッファ
	void SyncBufferToJoint();                          // 編集バッファ → Joint

	Matrix4x4 GetTargetWorldMatrix() const;

private:
	///************************* 定数 *************************///
	static constexpr const char* kModelRootDir = "Resources/Models";
	static constexpr float kPi = 3.14159265f;

private:
	///************************* メンバ変数 *************************///

	// 全パネル共有コンテキスト
	MotionEditorContext context_;

	// UIパネルのリスト
	std::vector<std::unique_ptr<IMotionEditorPanel>> panels_;

	// プレビュー・描画系
	WorldTransform previewTransform_;
	std::unique_ptr<Line> lineDrawer_;

#ifdef USE_IMGUI
	std::unique_ptr<Object3d> boneObj_; // ICO.obj 表示用
	GizmoController gizmoCtrl_;         // ギズモ操作用コントローラー
	std::vector<std::unique_ptr<BoneGizmable>> boneGizmables_; // ピッキング用
	std::vector<WorldTransform> boneWorldTransforms_; // ボーン描画用 WT
#endif

	// モデル読み込み・保存系
	std::string loadFileName_ = "";
	std::string loadDirectory_ = "";
	std::vector<std::string> animNameList_;
	int animNameIndex_ = -1;
	std::string loadAnimName_ = "";

	std::string selectedAnimKey_ = ""; // animationCache_ のキー
	std::string savePath_ = "Resources/TestBinary/edited_motion.anim";
	FileBrowserState binaryBrowser_;
	std::string saveMsg_ = "";

	// レイアウト用
	float timelineH_ = 240.0f;
	float timelineZoom_ = 80.0f;
	float bonePanelW_ = 185.0f;

	// ボーンギズモドラッグ中判定
	bool draggingBone_ = false;
	QuaternionTransform boneSnap_ = {};
};