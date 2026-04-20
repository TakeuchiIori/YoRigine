#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>

// Engine
#include "Object3d/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include "Editor/Command/CommandHistory.h"
#include "Debugger/DopeSheet/DopeSheetEditor.h"
#include <Graphics/Drawer/LineManager/Line.h>
#include "Object3D/ObjectManager.h" 

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

// ============================================================
//  選択中キーフレーム情報
// ============================================================
enum class KFChannel { Translate, Rotate, Scale };

struct SelectedKF
{
	std::string boneName = "";
	int         index = -1;
	KFChannel   channel = KFChannel::Translate;

	bool IsValid() const { return !boneName.empty() && index >= 0; }
	void Clear() { boneName = ""; index = -1; }
};

/// <summary>
/// モーションを編集するエディタ
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
	bool IsDrawBone() const { return isDrawBone_; }
	int GetTargetObjectId() const { return targetObjectId_; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	///************************* ギズモ操作用インターフェース *************************///

	// ボーンの現在のワールド行列を取得
	Matrix4x4 GetJointWorldMatrix(const std::string& boneName) const;

	// ギズモで操作された新しいワールド行列をローカルに変換し、ジョイントとUIに適用する
	void ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat);

private:
	///************************* その他描画 *************************///
	void DrawMenuBar();
	void DrawToolbar();
	void DrawBonePanel();
	void DrawPropertyPanel();
	void DrawTimeline();
	void DrawStatusBar();

	///************************* ポップアップ *************************///
	void DrawSaveLoadPopup();
	void DrawFileBrowser(FileBrowserState& state, const char* title);

	///************************* 内部ヘルパー関数 *************************///
#ifdef USE_IMGUI
	/// 1 ボーン × 1 チャンネルの行を描画
	void DrawKFRow(ImDrawList* dl,
		const std::string& boneName, KFChannel ch,
		float rowY, float canvasX, float canvasW, float labelW);
#endif
	float TimeToX(float t, float cx) const { return cx + t * timelineZoom_ - timelineScroll_; }
	float XToTime(float x, float cx) const { return (x - cx + timelineScroll_) / timelineZoom_; }


	///************************* DopeSheet *************************///
	void RebuildTracks();
	void ApplyTracksToMotion();

	///************************* キーフレーム操作 *************************///
	void InsertKeyframe(const std::string& bone, float time);
	void DeleteKeyframe(const std::string& bone, float time);
	void MoveKeyframe(const std::string& bone, KFChannel ch, int idx, float newTime);

	// ★追加: 全ボーンの現在のトランスフォームを一括保存
	void SavePose(float time);
	// ★追加: 特定のトランスフォームから直接キーフレームを挿入
	void InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr);

	///************************* ボーン操作 *************************///
	void   SetJointTransform(const std::string& bone, const QuaternionTransform& tr);
	Joint* FindJoint(const std::string& name) const;

	///************************* ユーティリティ *************************///
	static std::string AnimDisplayName(const std::string& cacheKey);
	static std::vector<std::string> FetchAnimationNames(const std::string& fullPath);
	std::vector<std::filesystem::directory_entry> GetDirectoryEntries(
		const std::string& dir, const std::string& ext) const;

	QuaternionTransform BufferToTransform() const;
	void SyncJointToBuffer(const std::string& bone);  // Joint の現在値 → 編集バッファ
	void SyncBufferToJoint();                          // 編集バッファ → Joint

	Object3d* GetTargetObject() const {
		if (targetObjectId_ != -1) {
			return ObjectManager::GetInstance()->GetObject3dById(targetObjectId_);
		}
		return nullptr;
	}

	// ★追加: 対象オブジェクトのワールド行列を取得する
	Matrix4x4 GetTargetWorldMatrix() const {
		if (targetObjectId_ != -1) {
			auto* placedObj = ObjectManager::GetInstance()->GetObjectById(targetObjectId_);
			if (placedObj && placedObj->worldTransform) {
				return placedObj->worldTransform->GetMatWorld();
			}
		}
		return previewTransform_.matWorld_;
	}

private:
	///************************* 定数 *************************///
	static constexpr const char* kModelRootDir = "Resources/Models";
	static constexpr float kRowH = 18.0f;    // タイムライン 1 行の高さ
	static constexpr float kLabelW = 150.0f;   // ボーン名ラベル幅
	static constexpr float kPi = 3.14159265f;

private:
	///************************* メンバ変数 *************************///

	// ドープシートエディ���
	DopeSheet::DopeSheetEditor dopeSheet_;
	std::vector<DopeSheet::DopeTrack> tracks_;
	std::vector<std::pair<std::string, int>> trackBoneMap_;
	bool tracksDirty_ = false;    // ドープシートの内容が Motion に反映されていない状態
	int fps_ = 60;

	Camera* camera_ = nullptr;

	// プレビュー
	int targetObjectId_ = -1;
	WorldTransform            previewTransform_;
	Motion* currentMotion_ = nullptr;

	// Undo / Redo
	CommandHistory history_;

	// モデル読み込み
	std::string      loadFileName_ = "";
	std::string      loadDirectory_ = "";
	std::vector<std::string> animNameList_;
	int              animNameIndex_ = -1;
	std::string      loadAnimName_ = "";
	bool             showLoadPopup_ = false;
	std::unique_ptr<Line> lineDrawer_;
	bool             isDrawBone_ = false;

#ifdef USE_IMGUI
	std::unique_ptr<Object3d> boneObj_; // ICO.obj 表示用
	GizmoController gizmoCtrl_;         // ギズモ操作用コントローラー
	std::vector<std::unique_ptr<BoneGizmable>> boneGizmables_; // ピッキング用
	std::vector<WorldTransform> boneWorldTransforms_; // ボーン描画用 WT（コマンドリストが参照するためフレーム間で生存させる）
#endif

	// 再生状態の管理 (UI用)
	bool isPlaying_ = true;
	bool isLoop_ = true;

	// モーション選択
	std::string selectedAnimKey_ = "";    // animationCache_ のキー

	// 保存・読み込み
	std::string      savePath_ = "Resources/TestBinary/edited_motion.anim";
	FileBrowserState binaryBrowser_;
	bool             showSavePopup_ = false;
	std::string      saveMsg_ = "";

	// ボーン編集
	std::string         selBone_ = "";
	float               editT_[3] = { 0,0,0 };   // Translate
	float               editR_[3] = { 0,0,0 };   // Rotate (度)
	float               editS_[3] = { 1,1,1 };   // Scale
	QuaternionTransform boneSnap_ = {};
	bool                draggingBone_ = false;

	// タイムライン
	float     scrubTime_ = 0.0f;
	float     timelineZoom_ = 80.0f;   // px / 秒
	float     timelineScroll_ = 0.0f;
	SelectedKF selKF_;

	// KF 値編集スナップショット
	Vector3    kfSnapT_ = {};
	Quaternion kfSnapR_ = { 0,0,0,1 };
	Vector3    kfSnapS_ = { 1,1,1 };

	// KF ドラッグ移動
	bool  draggingKF_ = false;
	float dragKFOrigTime_ = 0.0f;
	float dragKFStartMX_ = 0.0f;

	// レイアウト
	float bonePanelW_ = 185.0f;
	float timelineH_ = 240.0f;

	// ステータスバー
	std::string statusMsg_ = "Ready";
};