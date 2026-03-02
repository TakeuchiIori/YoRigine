#pragma once

#include <json.hpp>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include "ParticleSetting.h"

/// <summary>
/// パーティクル用のJson管理クラス
/// </summary>
class ParticleJsonManager {
public:
	static ParticleJsonManager& GetInstance() {
		static ParticleJsonManager instance;
		return instance;
	}

	// 保存・読み込み
	bool SaveSettings(const std::string& systemName, const ParticleSetting& settings);
	bool LoadSettings(const std::string& systemName, ParticleSetting& settings);

	// プリセット管理
	bool SavePreset(const std::string& presetName, const ParticleSetting& settings);
	bool LoadPreset(const std::string& presetName, ParticleSetting& settings);

	// ファイル管理
	std::vector<std::string> GetAvailableSettings() const;
	std::vector<std::string> GetAvailablePresets() const;
	bool DeleteSettings(const std::string& systemName);
	bool DeletePreset(const std::string& presetName);

	// ディレクトリ設定
	void SetBaseDirectory(const std::string& path) { baseDirectory_ = path; }
	std::string GetSettingsPath(const std::string& systemName) const;
	std::string GetPresetPath(const std::string& presetName) const;

private:
	std::string baseDirectory_ = "Resources/Json/Particles/";

	// 内部ヘルパー
	bool SaveToFile(const std::string& filePath, const ParticleSetting& settings);
	bool LoadFromFile(const std::string& filePath, ParticleSetting& settings);
	void EnsureDirectoryExists(const std::string& directory);
	std::vector<std::string> GetFilesInDirectory(const std::string& directory) const;
};