#pragma once

// Engine
#include "Systems/Camera/Camera.h"
#include <Loaders/Json/JsonManager.h>
// C++
#include <memory>
#include <string>

/// <summary>
/// Picture-in-Picture (PiP) カメラサブシステム。
///
/// ■ 役割
///   メインのシーン描画とは別に独立した視点 (PipCameraSystem::pipCamera_) を
///   持ち、その視点でレンダリングした結果を ImGui ウィンドウに表示する。
///
/// ■ Phase 1a (現状)
///   - 専用カメラ + 専用 RTV/DSV を確保 (RtvManager / DsvManager 経由)
///   - ImGui ウィンドウから位置/回転/FOV を編集
///   - プレビュー画面はクリアカラーのみ (2nd レンダーパス未接続)
///
/// ■ Phase 1b (今後)
///   - MyGame::Draw に 2nd オフスクリーンパスを差し込み
///   - GameScene を「指定カメラで 3D だけ描画」に対応させて pipCamera_ で再描画
/// </summary>
class PipCameraSystem
{
public:
	// シングルトン
	static PipCameraSystem* GetInstance();

	void Initialize();
	void Update();
	void Finalize();

	// ImGui ウィンドウ描画 (Editor::RegisterGameUI から呼ばれる)
	void DrawImGuiWindow();

	// ───────────────────────────────────────────────
	// 描画パス接続用
	// ───────────────────────────────────────────────
	bool IsEnabled()  const { return enabled_; }
	uint32_t GetWidth()  const { return rtWidth_; }
	uint32_t GetHeight() const { return rtHeight_; }
	YoRigine::Camera*  GetCamera()       { return pipCamera_.get(); }

	const std::string& GetRTName()    const { return rtName_; }
	const std::string& GetDSVName()   const { return dsvName_; }

	// PiP パス前に対象カメラ (シーンカメラ) を退避して PiP の値を流し込む。
	// 内部で UpdateMatrix + Update を呼ぶので GPU バッファも PiP のものに書き換わる。
	void ApplyToCamera(YoRigine::Camera* target);

	// PiP パス後に元の値を書き戻して GPU バッファも復元する。
	void RestoreCamera(YoRigine::Camera* target);

	// シーンカメラの位置/回転/FOV をコピーして PiP カメラに反映する (動作確認用)
	void SnapToSceneCamera(const YoRigine::Camera* sceneCamera);

private:
	PipCameraSystem() = default;
	~PipCameraSystem() = default;
	PipCameraSystem(const PipCameraSystem&) = delete;
	PipCameraSystem& operator=(const PipCameraSystem&) = delete;

	// RTV/DSV を作成 (Initialize から呼ばれる)
	void CreateRenderResources();
	void InitJson();

private:
	// PiP 専用カメラ
	std::unique_ptr<YoRigine::Camera> pipCamera_;
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;

	// 表示する/しないフラグ (ImGui のチェックボックスで制御)
	bool enabled_ = false;

	// 描画解像度 (中解像度 960x540)
	uint32_t rtWidth_  = 960;
	uint32_t rtHeight_ = 540;

	// RtvManager / DsvManager に登録する一意の名前
	const std::string rtName_  = "PipRT";
	const std::string dsvName_ = "PipDepth";

	// 一度作成済みかどうか (Initialize の冪等性確保用)
	bool resourcesCreated_ = false;

	// ApplyToCamera で退避したカメラの元状態 (RestoreCamera で書き戻す)
	struct SavedCamera {
		EulerTransform transform;
		float fovY;
		float aspect;
		float nearClip;
		float farClip;
		bool  valid = false;
	} saved_;
};
