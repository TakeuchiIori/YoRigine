#pragma once
#ifdef USE_IMGUI

#include <imgui.h>
#include <imgui_internal.h>   // ImGuiID, DockBuilder API
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <fstream>

// =============================================================================
//  Editor
//  エンジンの ImGui エディタ全体を管理するシングルトン
//  テーマ : "Black Gold" ― 漆黒 × 金箔アクセント
//
//  新機能 (ImGui 1.92):
//    ・SeparatorText  による見出し付き区切り線
//    ・TabBar / TabItem の DimmedSelected 対応
//    ・ToolTip / HoverDelay API 統一
//    ・ステータスバー (常時最下段固定)
//    ・FPS / フレームタイム オーバーレイ
// =============================================================================
class Editor
{
public:
	///======================== 基本関数 ============================///
	static Editor* GetInstance();
	void Initialize();
	void Finalize();
	void Draw();

	// --- ログウィンドウ描画 (RegisterGameUI で登録して使う) ---
	void DrawLog();

	// --- GameUI 登録 / 解除 ---
	void RegisterGameUI(
		const std::string& name,
		std::function<void()> drawFunc,
		const std::string& sceneName = "AllScene");

	void UnregisterGameUI(const std::string& name);

	// --- コールバック ---
	void SetSceneChangeCallback(std::function<void(const std::string&)> cb)
	{
		sceneChangeCallback_ = cb;
	}

	void RegisterMenuBar(std::function<void()> cb)
	{
		menuCallbacks_.push_back(cb);
	}

	void SetGizmoDrawCallback(std::function<void()> cb)
	{
		gizmoDrawCallback_ = cb;
	}

	// --- ゲームビュー情報 ---
	ImVec2 GetGameViewSize()   const { return gameViewSize_; }
	ImVec2 GetGameViewPos()    const { return gameViewPos_; }
	ImVec2 GetGameWindowAvail()const { return gameWindowAvail_; }
	bool   GetShowEditor()     const { return showEditor_; }

	// =========================================================================
	//  スタイルヘルパー
	//  ― ゲームUI ウィンドウ内でも色統一したい場合に呼ぶ
	// =========================================================================

	static void SeparatorText(const char* label);

	// アクセントカラーのボタン (通常より目立たせたいとき)
	static bool Button(const char* label, ImVec2 size = ImVec2(0, 0));

	// 小さいアイコンボタン
	static bool IconButton(const char* icon, const char* tooltip = nullptr);

	// テキスト (見出しなど)
	static void Text(const char* fmt, ...);

	// トグルスイッチ風 Checkbox
	static bool ToggleButton(const char* label, bool* value);

	// 進捗バー
	static void ProgressBar(float fraction, ImVec2 size = ImVec2(-1, 0), const char* overlay = nullptr);

private:
	///======================== 内部処理 ============================///
	void DrawMenuBar();
	void DrawGameWindow();
	void DrawGameUIs();
	void DrawStatusBar();     // ← 新規: 最下段ステータスバー
	void SaveSettings();
	void LoadSettings();
	void ApplySettings();

	// DockSpace 初回レイアウトを組む (imgui.ini がない場合)
	void SetupDefaultDockLayout(ImGuiID dockspaceID);

private:
	///======================== 構造体 ==============================///
	struct GameUI {
		std::string           name;
		std::string           sceneName = "AllScene";
		std::function<void()> drawFunc;
		bool                  visible = true;
	};

	struct SavedSettings {
		bool visible = true;
		bool hasSettings = false;
	};

	///======================== シングルトン ========================///
	Editor() = default;
	static Editor* instance_;
	static bool    showEditor_;

	///======================== メンバ変数 ==========================///
	bool        isAllDrawEditor_ = false;
	bool        showFpsOverlay_ = true;   // FPS オーバーレイ表示フラグ
	bool        dockLayoutDone_ = false;  // デフォルトレイアウト適用済みフラグ
	bool        settingsLoaded_ = false;

	std::string currentScene_ = "Title";
	std::vector<std::string> sceneNames_ = { "Title", "Game", "Clear", "Develop" };

	std::unordered_map<std::string, GameUI>       gameUIs_;
	std::unordered_map<std::string, SavedSettings>savedSettings_;

	std::function<void(const std::string&)> sceneChangeCallback_;
	std::vector<std::function<void()>>      menuCallbacks_;

	ImGuiID dockspaceID_ = 0;

	ImVec2  gameViewSize_ = { 0, 0 };
	ImVec2  gameViewPos_ = { 0, 0 };
	ImVec2  gameWindowAvail_ = { 0, 0 };

	std::function<void()> gizmoDrawCallback_;
};

#endif // USE_IMGUI