#pragma once
#include "json.hpp"
#include <string>
#include <map>
#include <memory>

class CameraState;

// ============================================================
// カメラステートのプリセット管理クラス
// ============================================================
class CameraStatePresetManager {
public:
	static CameraStatePresetManager* GetInstance();

	// ============================================================
	// プリセット管理操作
	// ============================================================
	void SavePreset(const std::string& presetName, const std::string& stateType, const CameraState* state);
	std::unique_ptr<CameraState> LoadPreset(const std::string& presetName);
	void DeletePreset(const std::string& presetName);
	std::vector<std::string> GetPresetNames() const;

	// ============================================================
	// ファイル入出力
	// ============================================================
	void SaveToFile(const std::string& filepath);
	void LoadFromFile(const std::string& filepath);

	// ============================================================
	// エディタ描画
	// ============================================================
	void DrawPresetManagerGui();

private:
	CameraStatePresetManager() = default;
	~CameraStatePresetManager() = default;
	CameraStatePresetManager(const CameraStatePresetManager&) = delete;
	CameraStatePresetManager& operator=(const CameraStatePresetManager&) = delete;

	std::unique_ptr<CameraState> CreateStateFromType(const std::string& stateType);

	struct PresetData {
		std::string stateType;
		nlohmann::json data;
	};

	std::map<std::string, PresetData> presets_;
	std::string selectedPreset_;
};