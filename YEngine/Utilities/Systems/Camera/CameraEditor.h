#pragma once
#include <string>
#include <memory>

// ============================================================
// カメラエディタクラス
// ImGuiを使ったカメラ設定の編集や、ファイルセーブ・ロードを担う
// ============================================================
class CameraEditor {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Initialize();
	void Update();

	// ============================================================
	// ファイル入出力
	// ============================================================
	void LoadFileOrDefault(const std::string& filePath, const std::string& sceneType);
	void SaveFile(const std::string& filePath);
	void LoadFile(const std::string& filePath);

	// ============================================================
	// アクセッサ
	// ============================================================
	void SetFilePath(const std::string& path) { filePath_ = path; }
	const std::string& GetFilePath() const { return filePath_; }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void InitializeDefaults(const std::string& sceneType);

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	std::string selectedCameraName_;
	char newCameraName_[64] = "NewCamera";
	std::string filePath_ = "Resources/Json/VirtualCameraData/VirtualCameras.json";
};