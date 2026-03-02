#pragma once

#ifdef USE_IMGUI
#include <json.hpp>
#include <string>
#include "FileOperations/FileBrowser.h"

/// <summary>
/// YParticleSystem用のエディタクラス
/// システムの作成、編集、保存/読込を提供
/// </summary>
class YParticleEditor {
public:
	//=================================================================
	// シングルトン
	//=================================================================

	static YParticleEditor& GetInstance() {
		static YParticleEditor instance;
		return instance;
	}

	YParticleEditor(const YParticleEditor&) = delete;
	YParticleEditor& operator=(const YParticleEditor&) = delete;

	//=================================================================
	// エディタウィンドウ
	//=================================================================

	/// <summary>
	/// メインエディタウィンドウを表示
	/// </summary>
	void ShowEditorWindow();

	//=================================================================
	// ファイル操作
	//=================================================================

	bool SaveAllSystemsToFile(const std::string& filePath);
	bool LoadAllSystemsFromFile(const std::string& filePath);
	bool SaveSystemToFile(const std::string& systemName, const std::string& filePath);
	bool LoadSystemFromFile(const std::string& filePath);

	//=================================================================
	// JSON変換
	//=================================================================

	nlohmann::json SaveAllSystemsToJson() const;
	void LoadAllSystemsFromJson(const nlohmann::json& json);
	nlohmann::json SaveSystemToJson(const std::string& systemName) const;
	void LoadSystemFromJson(const nlohmann::json& json);

	//=================================================================
	// エディタ状態
	//=================================================================

	void SetSelectedSystem(const std::string& systemName) { selectedSystemName_ = systemName; }
	const std::string& GetSelectedSystem() const { return selectedSystemName_; }

private:
	YParticleEditor();
	~YParticleEditor() = default;

	//=================================================================
	// 内部ヘルパー関数
	//=================================================================

	void ShowSystemCreationUI();
	void ShowSystemListUI();
	void ShowSystemDetailUI();

	/// <summary>
	/// ファイル操作UI（FileBrowser 版）
	/// </summary>
	void ShowFileOperationsUI();

	//=================================================================
	// メンバ変数
	//=================================================================

	std::string selectedSystemName_;
	char newSystemNameBuffer_[128] = "";
	int newSystemMaxParticles_ = 1000;

	float systemListWidth_ = 200.0f;

	// JSONファイルのデフォルトディレクトリ
	const std::string jsonDirectory_ = "Resources/Json/YParticleSystems/";

	//=================================================================
	// FileBrowser（Save / Load / Single-Save / Single-Load）
	//=================================================================

	YoRigine::FileBrowser saveAllBrowser_;
	YoRigine::FileBrowser loadAllBrowser_;
	YoRigine::FileBrowser saveSingleBrowser_;
	YoRigine::FileBrowser loadSingleBrowser_;

	bool showSaveAllPopup_ = false;
	bool showLoadAllPopup_ = false;
	bool showSaveSinglePopup_ = false;
	bool showLoadSinglePopup_ = false;

	// ポップアップ内の新規ファイル名バッファ（各ブラウザ共用）
	char saveAsNameBuf_[256] = "";
};

#endif // USE_IMGUI