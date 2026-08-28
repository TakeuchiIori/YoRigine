#ifdef USE_IMGUI

#include <imgui_internal.h>

// Engine
#include "Editor.h"
#include "DirectXCommon.h"
#include "WinApp/WinApp.h"
#include "Debugger/Logger.h"
#include <SceneSystems/SceneManager.h>
#include <Debugger/ImGuiManager.h>
#include <IconsFontAwesome5.h>
#include <array>

namespace {

std::string InferEditorCategory(const std::string& name)
{
	auto contains = [&name](const char* token) {
		return name.find(token) != std::string::npos;
	};

	if (contains("パーティクル") || contains("VFX") || contains("GpuParticle") ||
		contains("複合エフェクト")) {
		return "エフェクト";
	}
	if (contains("カメラ") || contains("ライティング") || contains("ポストエフェクト") ||
		contains("トゥーン") || contains("輪郭線") || contains("PiP")) {
		return "レンダリング";
	}
	if (contains("モデル操作") || contains("オブジェクト") || contains("Area") ||
		contains("NavGrid") || contains("EventTrigger") || contains("スポーン") ||
		contains("フィールドエネミー")) {
		return "シーン";
	}
	if (contains("プレイヤー") || contains("バトルモード")) {
		return "ゲームプレイ";
	}
	if (contains("ログ") || contains("デバッグ") || contains("状態情報") ||
		contains("ゲーム時間") || contains("YWebAPI")) {
		return "デバッグ";
	}
	if (contains("当たり判定") || contains("JSON") || contains("UI管理") ||
		contains("テキスト") || contains("オーディオ")) {
		return "システム";
	}
	return "その他";
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  内部カラー定数 (ImGuiManager::GetAccentColor() と同値)
// ─────────────────────────────────────────────────────────────────────────────
namespace EditorTheme
{
	// ステータスバーの高さ
	static constexpr float kStatusBarH = 22.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  静的メンバ
// ─────────────────────────────────────────────────────────────────────────────
bool    Editor::showEditor_ = false;

// =============================================================================
//  シングルトン
// =============================================================================
Editor* Editor::GetInstance()
{
	static Editor instance;
	return &instance;
}

// =============================================================================
//  初期化
// =============================================================================
void Editor::Initialize()
{
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = "imgui.ini";

	auto current = SceneManager::GetInstance()->GetCurrentSceneName();
	if (!current.empty()) {
		currentScene_ = current;
	}

	LoadSettings();
	ApplySettings();
}

// =============================================================================
//  終了
// =============================================================================
void Editor::Finalize()
{
	SaveSettings();
	gameUIs_.clear();
}

// =============================================================================
//  メイン描画
// =============================================================================
void Editor::Draw()
{
	// F1 でエディタ表示トグル
	if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
		showEditor_ = !showEditor_;
	}
	if (!showEditor_) return;

	// -----------------------------------------------------------------
	//  フルスクリーン Host ウィンドウ
	// -----------------------------------------------------------------
	ImGuiViewport* vp = ImGui::GetMainViewport();

	// ステータスバー分だけ高さを減らす
	ImGui::SetNextWindowPos(vp->Pos);
	ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - EditorTheme::kStatusBarH));
	ImGui::SetNextWindowViewport(vp->ID);

	constexpr ImGuiWindowFlags kHostFlags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("##EditorHost", nullptr, kHostFlags);
	ImGui::PopStyleVar(3);

	DrawMenuBar();

	// ----- DockSpace -----
	dockspaceID_ = ImGui::GetID("EditorDockSpace");

	ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpace(dockspaceID_, ImVec2(0, 0), dockFlags);

	// 初回のみデフォルトレイアウトを構築
	if (!dockLayoutDone_) {
		// imgui.ini に既存レイアウトがあれば上書きしない
		if (ImGui::DockBuilderGetNode(dockspaceID_) == nullptr) {
			SetupDefaultDockLayout(dockspaceID_);
		}
		dockLayoutDone_ = true;
	}
	if (workspaceResetRequested_) {
		ResetWorkspace();
		workspaceResetRequested_ = false;
	}

	ImGui::End();

	// -----------------------------------------------------------------
	//  子ウィンドウ群
	// -----------------------------------------------------------------
	DrawGameWindow();
	DrawGameUIs();

	DrawStatusBar();
}

// =============================================================================
//  デフォルト DockLayout (imgui.ini 未存在時のみ)
// =============================================================================
void Editor::SetupDefaultDockLayout(ImGuiID dockspaceID)
{
	// DockBuilder API (imgui_internal.h)
	ImGui::DockBuilderRemoveNode(dockspaceID);
	ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

	dockMainID_ = dockspaceID;
	dockRightID_ = ImGui::DockBuilderSplitNode(
		dockMainID_, ImGuiDir_Right, 0.25f, nullptr, &dockMainID_);
	dockLeftID_ = ImGui::DockBuilderSplitNode(
		dockMainID_, ImGuiDir_Left, 0.20f, nullptr, &dockMainID_);
	dockBottomID_ = ImGui::DockBuilderSplitNode(
		dockMainID_, ImGuiDir_Down, 0.24f, nullptr, &dockMainID_);

	// ゲームビューを中央に配置
	ImGui::DockBuilderDockWindow(
		SceneManager::GetInstance()->GetCurrentSceneName().c_str(), dockMainID_);
	ImGui::DockBuilderDockWindow("オブジェクト一覧", dockLeftID_);
	ImGui::DockBuilderDockWindow("モデル操作", dockRightID_);
	ImGui::DockBuilderDockWindow("ログ", dockBottomID_);

	ImGui::DockBuilderFinish(dockspaceID);
}

// =============================================================================
//  メニューバー
// =============================================================================
void Editor::DrawMenuBar()
{
	// メニューバー背景をわずかにゴールドがかった黒に
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg,
		ImVec4(0.045f, 0.034f, 0.024f, 1.0f));

	if (ImGui::BeginMenuBar()) {

		// ---- エンジンロゴ的テキスト ----
		ImGui::PushFont(ImGuiManager::GetInstance()->GetFontLarge());
		ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
		ImGui::Text(ICON_FA_DICE_D6);   // Font Awesome: dice-d6 アイコン (元コメントは chess-king だったが実際のコードポイントは dice-d6)
		ImGui::PopStyleColor();
		ImGui::PopFont();
		ImGui::SameLine(0, 6);

		// ---- シーン切り替え ----
		if (ImGui::BeginMenu(ICON_FA_BARS "  シーン")) {
			ImGui::SeparatorText("シーン一覧");
			for (const auto& sceneName : sceneNames_) {
				bool selected = (currentScene_ == sceneName);
				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
				}
				if (ImGui::MenuItem(sceneName.c_str(), nullptr, selected)) {
					currentScene_ = sceneName;
					if (sceneChangeCallback_) {
						sceneChangeCallback_(sceneName);
					}
				}
				if (selected) {
					ImGui::PopStyleColor();
				}
			}
			ImGui::EndMenu();
		}

		// ---- Unity / Unreal 風のカテゴリ別パネルメニュー ----
		if (ImGui::BeginMenu(ICON_FA_LAYER_GROUP "  ウィンドウ")) {
			if (ImGui::MenuItem("標準レイアウトに戻す")) {
				workspaceResetRequested_ = true;
			}
			if (ImGui::MenuItem("全パネルを閉じる")) {
				for (auto& [name, ui] : gameUIs_) {
					ui.visible = false;
				}
			}
			ImGui::Separator();

			const auto currentScene = SceneManager::GetInstance()->GetCurrentSceneName();
			constexpr std::array<const char*, 7> kCategoryOrder = {
				"シーン", "ゲームプレイ", "レンダリング", "エフェクト",
				"システム", "デバッグ", "その他"
			};

			for (const char* category : kCategoryOrder) {
				std::vector<GameUI*> panels;
				for (auto& [name, ui] : gameUIs_) {
					if (ui.category != category) continue;
					if (ui.sceneName != "AllScene" && ui.sceneName != currentScene) continue;
					panels.push_back(&ui);
				}
				if (panels.empty()) continue;

				std::sort(panels.begin(), panels.end(),
					[](const GameUI* a, const GameUI* b) {
						return a->name < b->name;
					});

				if (ImGui::BeginMenu(category)) {
					for (auto* panel : panels) {
						ImGui::MenuItem(panel->name.c_str(), nullptr, &panel->visible);
					}
					ImGui::EndMenu();
				}
			}
			ImGui::EndMenu();
		}

		// ---- 表示 ----
		if (ImGui::BeginMenu(ICON_FA_PAINT_BRUSH "  表示")) {
			ImGui::SeparatorText("ビュー");
			if (ImGui::MenuItem(ICON_FA_EYE_SLASH "  エディターを非表示", "F1")) {
				showEditor_ = false;
			}
			ImGui::Separator();
			ImGui::SeparatorText("設定");
			if (ImGui::MenuItem(ICON_FA_SAVE "  設定をセーブ")) {
				SaveSettings();
			}
			ImGui::EndMenu();
		}

		// ---- 外部登録メニュー ----
		for (auto& cb : menuCallbacks_) {
			if (cb) cb();
		}

		// ---- 右端: 現在シーン表示 ----
		float rightOffset = ImGui::CalcTextSize(currentScene_.c_str()).x + 20.0f;
		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - rightOffset);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
		ImGui::Text(ICON_FA_GLOBE "  %s", currentScene_.c_str());
		ImGui::PopStyleColor();

		ImGui::EndMenuBar();
	}

	ImGui::PopStyleColor(); // MenuBarBg
}

// =============================================================================
//  ゲームウィンドウ (FinalResult 表示)
// =============================================================================
void Editor::DrawGameWindow()
{
	auto sceneName = SceneManager::GetInstance()->GetCurrentSceneName();

	// ゲームウィンドウは枠線なし・パディングなしで最大限広く
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	if (ImGui::Begin(
		sceneName.c_str(), nullptr,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoTitleBar))
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		gameWindowAvail_ = avail;

		auto dxCommon = YoRigine::DirectXCommon::GetInstance();
		if (dxCommon) {
			ImTextureID textureID = (ImTextureID)dxCommon->GetFinalResultGPUHandle().ptr;

			int   renderW = WinApp::GetInstance()->kClientWidth;
			int   renderH = WinApp::GetInstance()->kClientHeight;
			float aspect = (float)renderW / (float)renderH;
			float availAsp = avail.x / avail.y;

			ImVec2 imageSize;
			if (availAsp > aspect) {
				imageSize.y = avail.y;
				imageSize.x = imageSize.y * aspect;
			}
			else {
				imageSize.x = avail.x;
				imageSize.y = imageSize.x / aspect;
			}

			ImVec2 offset = {
				(avail.x - imageSize.x) * 0.5f,
				(avail.y - imageSize.y) * 0.5f
			};
			ImGui::SetCursorPos(ImVec2(
				ImGui::GetCursorPos().x + offset.x,
				ImGui::GetCursorPos().y + offset.y
			));

			gameViewPos_ = ImGui::GetCursorScreenPos();
			gameViewSize_ = imageSize;

			ImGui::Image(textureID, imageSize);

			if (gizmoDrawCallback_) {
				gizmoDrawCallback_();
			}
			for (auto& cb : extraGizmoDrawCallbacks_) {
				if (cb.fn) cb.fn();
			}

			// モデルをドラッグアンドドロップで配置するためのドロップターゲット
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_PATH")) {
					const char* path = (const char*)payload->Data;
					// 配置コールバックを呼び出す (SceneEditor::PlaceObject が呼ばれるようにする)
					YoRigine::SceneEditor::GetInstance()->PlaceObject(path);
				}
				ImGui::EndDragDropTarget();
			}
		}
	}
	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

// =============================================================================
//  GameUI 群
// =============================================================================
void Editor::DrawGameUIs()
{
	auto currentScene = SceneManager::GetInstance()->GetCurrentSceneName();

	for (auto& [name, ui] : gameUIs_) {
		if (!ui.visible)                       continue;
		if (!ui.drawFunc)                     continue;
		if (ui.sceneName != "AllScene" && ui.sceneName != currentScene) continue;

		// ウィンドウタイトルバーをゴールドで強調
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
			ImVec4(0.105f, 0.082f, 0.058f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_BorderShadow,
			ImVec4(0.852f, 0.682f, 0.196f, 0.10f));

		const ImGuiID defaultDock = GetDefaultDockForCategory(ui.category);
		if (defaultDock != 0) {
			ImGui::SetNextWindowDockID(defaultDock, ImGuiCond_FirstUseEver);
		}
		if (ImGui::Begin(name.c_str(), &ui.visible)) {
			ui.drawFunc();
		}
		ImGui::End();

		ImGui::PopStyleColor(2);
	}
}

// =============================================================================
//  ステータスバー (最下段 固定バー)
// =============================================================================
void Editor::DrawStatusBar()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();

	// ステータスバーの矩形
	ImVec2 barPos = ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - EditorTheme::kStatusBarH);
	ImVec2 barSize = ImVec2(vp->Size.x, EditorTheme::kStatusBarH);

	ImGui::SetNextWindowPos(barPos);
	ImGui::SetNextWindowSize(barSize);
	ImGui::SetNextWindowViewport(vp->ID);

	constexpr ImGuiWindowFlags kBarFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg,
		ImVec4(0.052f, 0.038f, 0.022f, 1.0f)); // ゴールドがかったバー背景

	ImGui::Begin("##StatusBar", nullptr, kBarFlags);

	// --- 左: シーン / エディタ状態 ---
	ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
	ImGui::Text(ICON_FA_BARS "  %s", currentScene_.c_str());
	ImGui::PopStyleColor();

	ImGui::SameLine(0, 16);
	ImGui::PushFont(ImGuiManager::GetInstance()->GetFontSmall());

	ImGui::TextDisabled("F1: エディタ表示切替");
	ImGui::PopFont();

	// --- 右: FPS ---
	ImGuiIO& io = ImGui::GetIO();
	char fpsBuf[32];
	snprintf(fpsBuf, sizeof(fpsBuf), ICON_FA_TACHOMETER_ALT "  %.0f fps / %.2f ms",
		io.Framerate, 1000.0f / (io.Framerate + 0.0001f));

	float fpsWidth = ImGui::CalcTextSize(fpsBuf).x + 10.0f;
	ImGui::SameLine(barSize.x - fpsWidth - 8.0f);

	// 30fps 以下は赤、60fps 以下はオレンジ、それ以上はゴールド
	ImVec4 fpsColor;
	if (io.Framerate < 30.0f)       fpsColor = ImVec4(0.90f, 0.30f, 0.20f, 1.0f); // 赤
	else if (io.Framerate < 60.0f)  fpsColor = ImVec4(0.95f, 0.65f, 0.10f, 1.0f); // 橙
	else                             fpsColor = ImGuiManager::GetAccentColor();

	ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
	ImGui::Text("%s", fpsBuf);
	ImGui::PopStyleColor();

	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
}
// =============================================================================
//  GameUI 登録 / 解除
// =============================================================================
void Editor::RegisterGameUI(
	const std::string& name,
	std::function<void()> drawFunc,
	const std::string& sceneName,
	const std::string& category,
	bool defaultVisible)
{
	GameUI newUI{
		name,
		sceneName,
		category.empty() ? InferEditorCategory(name) : category,
		std::move(drawFunc),
		defaultVisible,
		defaultVisible
	};

	if (settingsLoaded_) {
		auto it = savedSettings_.find(name);
		if (it != savedSettings_.end() && it->second.hasSettings) {
			newUI.visible = it->second.visible;
		}
	}
	gameUIs_[name] = newUI;
}

void Editor::UnregisterGameUI(const std::string& name)
{
	gameUIs_.erase(name);
}

// =============================================================================
//  設定 Save / Load / Apply
// =============================================================================
void Editor::SaveSettings()
{
	std::ofstream file("editor_settings.ini");
	if (!file.is_open()) return;

	file << "[Editor]\n";
	file << "LayoutVersion=" << kLayoutVersion << "\n";
	file << "ShowEditor=" << (showEditor_ ? "1" : "0") << "\n";
	file << "CurrentScene=" << currentScene_ << "\n\n";

	file << "[GameUI]\n";
	for (const auto& [name, ui] : gameUIs_) {
		file << name << "_visible=" << (ui.visible ? "1" : "0") << "\n";
	}

	file.close();
}

void Editor::LoadSettings()
{
	std::ifstream file("editor_settings.ini");
	if (!file.is_open()) {
		settingsLoaded_ = true;
		return;
	}

	std::string line, section;
	int loadedLayoutVersion = 0;

	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') continue;

		if (line.front() == '[' && line.back() == ']') {
			section = line.substr(1, line.length() - 2);
			continue;
		}

		size_t pos = line.find('=');
		if (pos == std::string::npos) continue;

		std::string key = line.substr(0, pos);
		std::string value = line.substr(pos + 1);

		if (section == "Editor") {
			if (key == "LayoutVersion")  loadedLayoutVersion = std::stoi(value);
			else if (key == "ShowEditor")     showEditor_ = (value == "1");
			else if (key == "CurrentScene")   currentScene_ = value;
		}
		else if (section == "GameUI") {
			size_t under = key.rfind('_');
			if (under == std::string::npos) continue;

			std::string uiName = key.substr(0, under);
			std::string property = key.substr(under + 1);

			if (property == "visible") {
				if (savedSettings_.find(uiName) == savedSettings_.end()) {
					savedSettings_[uiName] = SavedSettings();
				}
				savedSettings_[uiName].visible = (value == "1");
				savedSettings_[uiName].hasSettings = true;
			}
		}
	}

	// 旧形式は全パネルが初期表示だったため、その可視状態は引き継がない。
	if (loadedLayoutVersion != kLayoutVersion) {
		savedSettings_.clear();
		workspaceResetRequested_ = true;
	}

	settingsLoaded_ = true;
	file.close();
}

void Editor::ApplySettings()
{
	for (auto& [name, ui] : gameUIs_) {
		auto it = savedSettings_.find(name);
		if (it != savedSettings_.end() && it->second.hasSettings) {
			ui.visible = it->second.visible;
		}
	}
}

void Editor::ResetWorkspace()
{
	// 旧ウィンドウの浮動位置やDock参照も含めて一度消し、
	// 現在の標準ワークスペースを同じ状態から再構築する。
	ImGui::ClearIniSettings();

	for (auto& [name, ui] : gameUIs_) {
		ui.visible = ui.defaultVisible;
	}

	SetupDefaultDockLayout(dockspaceID_);
	dockLayoutDone_ = true;
	SaveSettings();
}

ImGuiID Editor::GetDefaultDockForCategory(const std::string& category) const
{
	if (category == "シーン") return dockLeftID_;
	if (category == "デバッグ" || category == "システム") return dockBottomID_;
	if (category == "エフェクト") return dockMainID_;
	return dockRightID_;
}

// =============================================================================
//  ログ描画
// =============================================================================
void Editor::DrawLog()
{
	// --- ツールバー ---
	SeparatorText(ICON_FA_COMMENTS "  ログ");

	// フィルタ (1.87+ InputTextWithHint)
	static char filterBuf[128] = {};
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##logfilter", ICON_FA_SEARCH "  フィルタ...", filterBuf, sizeof(filterBuf));

	ImGui::SameLine();
	if (ImGui::SmallButton(ICON_FA_ERASER "  クリア")) {
		LogSystem::Get().Clear(); // ※ Clear() メソッドがある前提
	}

	ImGui::Separator();

	// --- ログ本体 (子ウィンドウでスクロール) ---
	ImGui::BeginChild("##logscroll", ImVec2(0, 0), false,
		ImGuiWindowFlags_HorizontalScrollbar);

	const auto& logs = LogSystem::Get().GetLogs();
	std::string filter(filterBuf);

	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(logs.size()));

	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
			const auto& line = logs[i];

			// フィルタ適用
			if (!filter.empty() && line.find(filter) == std::string::npos) continue;

			// ログレベルに応じた色分け (先頭文字で判定)
			if (line.find("[ERROR]") != std::string::npos)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.25f, 1.0f));
			else if (line.find("[WARN]") != std::string::npos)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.15f, 1.0f));
			else if (line.find("[INFO]") != std::string::npos)
				ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
			else
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.83f, 0.78f, 1.0f));

			ImGui::TextUnformatted(line.c_str());
			ImGui::PopStyleColor();
		}
	}
	clipper.End();

	// 自動スクロール
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
}

// =============================================================================
//  "Black Gold" スタイルヘルパー実装
// =============================================================================

void Editor::SeparatorText(const char* label)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
	ImGui::SeparatorText(label);   // ImGui 1.89+ API
	ImGui::PopStyleColor();
}

bool Editor::Button(const char* label, ImVec2 size)
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
	bool result = ImGui::Button(label, size);
	ImGui::PopStyleColor(4);
	return result;
}

bool Editor::IconButton(const char* icon, const char* tooltip)
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());

	bool result = ImGui::Button(icon, ImVec2(24, 24));

	ImGui::PopStyleColor(4);

	// ImGui 1.91+ BeginItemTooltip
	if (tooltip && ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted(tooltip);
		ImGui::EndTooltip();
	}

	return result;
}

void Editor::Text(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::PushStyleColor(ImGuiCol_Text, ImGuiManager::GetAccentColor());
	ImGui::TextV(fmt, args);
	ImGui::PopStyleColor();
	va_end(args);
}

bool Editor::ToggleButton(const char* label, bool* value)
{
	// オン/オフで色を切り替えるトグル
	ImVec4 btnColor = *value ? ImGuiManager::GetAccentColor() : ImGuiManager::GetBgDeep();
	ImVec4 textColor = *value ? ImGuiManager::GetAccentColor() : ImVec4(0.55f, 0.50f, 0.42f, 1.0f);

	ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_Text, textColor);

	bool clicked = ImGui::Button(label);
	if (clicked) *value = !*value;

	ImGui::PopStyleColor(4);
	return clicked;
}

void Editor::ProgressBar(float fraction, ImVec2 size, const char* overlay)
{
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGuiManager::GetAccentColor());
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGuiManager::GetAccentColor());
	ImGui::ProgressBar(fraction, size, overlay);
	ImGui::PopStyleColor(2);
}

#endif // USE_IMGUI
