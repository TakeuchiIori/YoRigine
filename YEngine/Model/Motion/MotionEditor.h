#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>
#include <map>

// Engine
#include "Object3d/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include "Editor/Command/CommandHistory.h"

// Math
#include "Vector3.h"
#include "Quaternion.h"

class Camera;
class Motion;
class Joint;
class Skeleton;

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
	void Initialize();
	void Update();
	void Draw(Camera* camera);
	void ShowEditor();

private:
	///************************* その他描画 *************************///
	void DrawMenuBar();
	void DrawToolbar();
	void DrawBonePanel();
	void DrawPropertyPanel();
	void DrawTimeline();
	void DrawStatusBar();

	///************************* ポップアップ *************************///
	void DrawModelLoadPopup();
	void DrawSaveLoadPopup();
	void DrawFileBrowser(FileBrowserState& state, const char* title);

	///************************* 内部ヘルパー関数 *************************///
#ifdef USE_IMGUI
	/// 1 ボーン × 1 チャンネルの行を描画（KF ひし形 + クリック/ドラッグ判定）
	void DrawKFRow(ImDrawList* dl,
		const std::string& boneName, KFChannel ch,
		float rowY, float canvasX, float canvasW, float labelW);
#endif
	float TimeToX(float t, float cx) const { return cx + t * timelineZoom_ - timelineScroll_; }
	float XToTime(float x, float cx) const { return (x - cx + timelineScroll_) / timelineZoom_; }

	///************************* キーフレーム操作 *************************///
	void InsertKeyframe(const std::string& bone, float time);
	void DeleteKeyframe(const std::string& bone, float time);
	void MoveKeyframe(const std::string& bone, KFChannel ch, int idx, float newTime);


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

private:
	///************************* 定数 *************************///
	static constexpr const char* kModelRootDir = "Resources/Models";
	static constexpr float kRowH = 18.0f;    // タイムライン 1 行の高さ
	static constexpr float kLabelW = 150.0f;   // ボーン名ラベル幅
	static constexpr float kPi = 3.14159265f;

private:
	///************************* メンバ変数 *************************///

	// プレビュー
	std::unique_ptr<Object3d> previewObject_;
	WorldTransform            previewTransform_;
	Motion* currentMotion_ = nullptr;

	// Undo / Redo
	CommandHistory history_;

	// モデル読み込み
	std::string      loadFileName_ = "";
	std::string      loadDirectory_ = "";
	FileBrowserState modelBrowser_;
	std::vector<std::string> animNameList_;
	int              animNameIndex_ = -1;
	std::string      loadAnimName_ = "";
	bool             showLoadPopup_ = false;

	// モーション選択
	std::string selectedAnimKey_ = "";    // animationCache_ のキー

	// 保存・読み込み
	std::string      savePath_ = "Resources/Binary/edited_motion.anim";
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