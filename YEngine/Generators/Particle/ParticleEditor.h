#pragma once

#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

#include "ParticleImGuiHelper.h"
#include "ParticleJsonManager.h"
#include <string>
#include <memory>
#include <vector>

class ParticleSystem;
/// <summary>
/// パーティクルエディタークラス
/// </summary>
class ParticleEditor {
public:
	///************************* 基本関数 *************************///
	static ParticleEditor& GetInstance() {
		static ParticleEditor instance;
		return instance;
	}
	void Initialize();
	void ShowEditor();
	void RegisterSystem(const std::string& name);
	void LoadAllSystems();
private:
	///************************* 内部処理 *************************///
	// UI表示メソッド
	void ShowMenuBar();
	void ShowSystemSelector();
	void ShowTabBar();
	void ShowPropertiesTab();
	void ShowPreviewTab();
	void ShowFilesTab();
	void ShowPerformanceTab();

	// ファイル操作
	void ShowFileOperations();
	void ShowPresetManager();
	void SaveCurrentSystem();
	void LoadCurrentSystem();
	void SavePreset(const std::string& name);
	void LoadPreset(const std::string& name);

	// プレビュー制御
	void ShowPreviewControls();
	void EmitParticles(int count);
	void ResetSystem();

	// 通知システム
	void AddNotification(const std::string& message, bool isError = false);
	void UpdateNotifications();

	// ユーティリティ
	void UpdateCurrentSystem();
	void OnSettingsChanged();

private:
	///************************* メンバ変数 *************************///
	// UI状態
	bool isOpen_ = true;
	std::string selectedSystem_;
	ParticleSystem* currentSystem_ = nullptr;

	// タブ状態
	int selectedTab_ = 0;
	enum TabType {
		TAB_PROPERTIES = 0,
		TAB_PREVIEW,
		TAB_FILES,
		TAB_PERFORMANCE
	};

	// プレビュー設定
	Vector3 previewPosition_ = { 0, 0, 0 };
	bool enablePreview_ = false;

	// ファイル管理
	char newPresetName_[128] = "";
	bool showSaveDialog_ = false;
	bool showLoadDialog_ = false;

	// 通知システム
	struct Notification {
		std::string message;
		bool isError;
		float timeLeft;
	};
	std::vector<Notification> notifications_;

	// ヘルパークラス
	std::unique_ptr<ParticleImGuiHelper> imguiHelper_;
	ParticleJsonManager* jsonManager_;
};