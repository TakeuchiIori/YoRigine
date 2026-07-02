#ifdef USE_IMGUI

#include "YParticleEditor.h"
#include "YParticleModuleFactory.h"
#include "YParticleManager.h"
#include "YEmitterGroupEditor.h"

#include <IconsFontAwesome5.h>
#include "imgui.h"
#include <fstream>
#include <algorithm>
#include <cmath>

//=================================================================
// 内部ヘルパー: 垂直分割線（シンプル版）
//=================================================================

static void Splitter(float thickness, float* size1, float min_size1, float min_size2, float height = -1.0f) {
	ImVec2 backup_pos = ImGui::GetCursorPos();
	if (height < 0)
		height = ImGui::GetContentRegionAvail().y;

	ImGui::Button("##vsplitter", ImVec2(thickness, height));
	if (ImGui::IsItemActive()) {
		float delta = ImGui::GetIO().MouseDelta.x;
		if (delta != 0.0f) {
			*size1 += delta;
			if (*size1 < min_size1) *size1 = min_size1;
			if (*size1 > ImGui::GetContentRegionAvail().x - min_size2 - thickness)
				*size1 = ImGui::GetContentRegionAvail().x - min_size2 - thickness;
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	ImGui::SetCursorPos(backup_pos);
}

//=================================================================
// コンストラクタ：FileBrowser を初期化
//=================================================================

YParticleEditor::YParticleEditor()
	: saveAllBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
	, loadAllBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
	, saveSingleBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List)
	, loadSingleBrowser_(jsonDirectory_, { ".json" }, YoRigine::FileBrowser::DisplayMode::List) {
	// Save All: 既存ファイルをクリックで上書き保存
	saveAllBrowser_.SetOnFileSelected([this](const std::string& path) {
		bool ok = SaveAllSystemsToFile(path);
		showSaveAllPopup_ = false;
		SetNotification(ok, ok ? "全システム保存完了: " + path : "保存失敗: " + path);
		});

	// Load All: ファイルをクリックでロード
	loadAllBrowser_.SetOnFileSelected([this](const std::string& path) {
		bool ok = LoadAllSystemsFromFile(path);
		showLoadAllPopup_ = false;
		SetNotification(ok, ok ? "全システム読み込み完了: " + path : "読み込み失敗: " + path);
		});

	// Save Selected: 既存ファイルをクリックで上書き保存
	saveSingleBrowser_.SetOnFileSelected([this](const std::string& path) {
		if (!selectedSystemName_.empty()) {
			bool ok = SaveSystemToFile(selectedSystemName_, path);
			SetNotification(ok, ok ? "保存完了: " + path : "保存失敗: " + path);
		}
		showSaveSinglePopup_ = false;
		});

	// Load Single: ファイルをクリックでロード
	loadSingleBrowser_.SetOnFileSelected([this](const std::string& path) {
		bool ok = LoadSystemFromFile(path);
		showLoadSinglePopup_ = false;
		SetNotification(ok, ok ? "読み込み完了: " + path : "読み込み失敗: " + path);
		});
}

//=================================================================
// 通知ヘルパー
//=================================================================

void YParticleEditor::SetNotification(bool success, const std::string& message) {
	saveNotifySuccess_ = success;
	saveNotifyMessage_ = message;
	saveNotifyTimer_ = 3.0f;   // 3秒間表示
}

/// <summary>
/// トースト通知をウィンドウ下部にオーバーレイ描画する
/// ShowEditorWindow() の先頭で毎フレーム呼ぶ
/// </summary>
void YParticleEditor::ShowSaveNotification() {
	if (saveNotifyTimer_ <= 0.0f) return;

	saveNotifyTimer_ -= ImGui::GetIO().DeltaTime;

	// フェードアウト（残り0.5秒から）
	float alpha = std::min(1.0f, saveNotifyTimer_ / 0.5f);
	alpha = std::min(alpha, 1.0f);

	ImVec4 col = saveNotifySuccess_
		? ImVec4(0.2f, 1.0f, 0.3f, alpha)
		: ImVec4(1.0f, 0.3f, 0.2f, alpha);

	const char* icon = saveNotifySuccess_ ? ICON_FA_CHECK :ICON_FA_TIMES;
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	ImGui::Text("%s%s", icon, saveNotifyMessage_.c_str());
	ImGui::PopStyleColor();
	ImGui::Separator();
}

//=================================================================
// エディタウィンドウ
//=================================================================

void YParticleEditor::ShowEditorWindow() {

	// ── トースト通知（最上部に表示）──────────────────────────
	ShowSaveNotification();

	// メニューバー
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("保存・読み込み設定")) {
			if (ImGui::MenuItem("全てのシステムを保存")) {
				saveAllBrowser_.Scan();
				// 全保存のデフォルト名は空にしておく
				saveAsNameBuf_[0] = '\0';
				showSaveAllPopup_ = true;
			}
			if (ImGui::MenuItem("全てのシステムを読み込み")) {
				loadAllBrowser_.Scan();
				showLoadAllPopup_ = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("選択しているシステムを保存")) {
				if (!selectedSystemName_.empty()) {
					saveSingleBrowser_.Scan();
					// ★ テンプレート名: 選択中のシステム名 + ".json"
					std::string defaultName = selectedSystemName_ + ".json";
					strncpy_s(saveAsNameBuf_, defaultName.c_str(), sizeof(saveAsNameBuf_));
					showSaveSinglePopup_ = true;
				}
			}
			if (ImGui::MenuItem("システムの読み込み")) {
				loadSingleBrowser_.Scan();
				showLoadSinglePopup_ = true;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	float availHeight = ImGui::GetContentRegionAvail().y;

	// 左側: システム一覧
	ImGui::BeginChild("SystemList", ImVec2(systemListWidth_, 0), true);
	ShowSystemListUI();
	ImGui::EndChild();

	ImGui::SameLine();

	Splitter(4.0f, &systemListWidth_, 120.0f, 300.0f, availHeight);

	ImGui::SameLine();

	// 右側: システム詳細とファイル操作
	ImGui::BeginChild("SystemDetail", ImVec2(0, 0), true);

	if (ImGui::BeginTabBar("タブ")) {
		if (ImGui::BeginTabItem("既存パーティクルシステムの詳細設定")) {
			ShowSystemDetailUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("新規パーティクルシステムを作成")) {
			ShowSystemCreationUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("エミッタグループ")) {
			YEmitterGroupEditor::GetInstance().ShowEditor();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("ファイル操作")) {
			ShowFileOperationsUI();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

//=================================================================
// ファイル操作
//=================================================================

bool YParticleEditor::SaveAllSystemsToFile(const std::string& filePath) {
	try {
		nlohmann::json json = SaveAllSystemsToJson();
		std::ofstream file(filePath);
		if (!file.is_open()) return false;
		file << json.dump(4);
		return true;
	}
	catch (const std::exception&) { return false; }
}

bool YParticleEditor::LoadAllSystemsFromFile(const std::string& filePath) {
	try {
		std::ifstream file(filePath);
		if (!file.is_open()) return false;
		nlohmann::json json;
		file >> json;
		LoadAllSystemsFromJson(json);
		return true;
	}
	catch (const std::exception&) { return false; }
}

bool YParticleEditor::SaveSystemToFile(const std::string& systemName, const std::string& filePath) {
	try {
		nlohmann::json json = SaveSystemToJson(systemName);
		std::ofstream file(filePath);
		if (!file.is_open()) return false;
		file << json.dump(4);
		return true;
	}
	catch (const std::exception&) { return false; }
}

bool YParticleEditor::LoadSystemFromFile(const std::string& filePath) {
	try {
		std::ifstream file(filePath);
		if (!file.is_open()) return false;
		nlohmann::json json;
		file >> json;
		LoadSystemFromJson(json);
		return true;
	}
	catch (const std::exception&) { return false; }
}

//=================================================================
// JSON変換
//=================================================================

nlohmann::json YParticleEditor::SaveAllSystemsToJson() const {
	nlohmann::json json;
	json["version"] = "1.0";
	json["systems"] = nlohmann::json::array();

	auto& manager = YParticleManager::GetInstance();
	for (const auto& name : manager.GetAllSystemNames()) {
		json["systems"].push_back(SaveSystemToJson(name));
	}
	return json;
}

void YParticleEditor::LoadAllSystemsFromJson(const nlohmann::json& json) {
	if (!json.contains("systems")) return;
	for (const auto& systemJson : json["systems"]) {
		LoadSystemFromJson(systemJson);
	}
}

nlohmann::json YParticleEditor::SaveSystemToJson(const std::string& systemName) const {
	nlohmann::json json;
	auto& manager = YParticleManager::GetInstance();
	auto* system = manager.GetSystem(systemName);
	if (!system) return json;

	json["name"] = system->GetName();
	json["maxParticles"] = system->GetMaxParticles();
	json["texture"] = system->GetTextureFilePath();
	json["isRelative"] = system->IsRelative();
	json["billboardType"] = system->GetBillboardTypeAsUInt();
	json["BlendMode"] = static_cast<int>(system->GetBlendMode());
	json["Lighting"] = system->IsEnableLight();
	json["softParticle"] = system->IsSoftParticle();
	json["softFadeDistance"] = system->GetSoftFadeDistance();
	json["emissiveIntensity"] = system->GetEmissiveIntensity();
	json["mesh"]["type"] = system->GetCurrentMeshType();
	json["mesh"]["params"] = nlohmann::json::array();
	const float* params = system->GetMeshParams();
	for (int i = 0; i < 4; ++i) json["mesh"]["params"].push_back(params[i]);

	json["spawnModules"] = nlohmann::json::array();
	for (const auto& module : system->GetSpawnModules()) {
		nlohmann::json moduleJson;
		moduleJson["type"] = module->GetTypeName();
		moduleJson["name"] = module->GetName();
		module->SaveToJson(moduleJson["data"]);
		json["spawnModules"].push_back(moduleJson);
	}

	json["updateModules"] = nlohmann::json::array();
	for (const auto& module : system->GetUpdateModules()) {
		nlohmann::json moduleJson;
		moduleJson["type"] = module->GetTypeName();
		moduleJson["name"] = module->GetName();
		module->SaveToJson(moduleJson["data"]);
		json["updateModules"].push_back(moduleJson);
	}

	return json;
}

void YParticleEditor::LoadSystemFromJson(const nlohmann::json& json) {
	if (!json.contains("name")) return;

	std::string name = json["name"];
	uint32_t maxP = json.value("maxParticles", 1000);

	auto& manager = YParticleManager::GetInstance();
	auto* system = manager.CreateSystem(name, maxP);
	if (!system) return;

	if (json.contains("texture"))      system->SetTexture(json["texture"]);
	if (json.contains("isRelative"))   system->SetRelative(json["isRelative"]);
	if (json.contains("billboardType"))
		system->SetBillboardType(static_cast<BillboardType>(json["billboardType"].get<uint32_t>()));
	if (json.contains("BlendMode"))
		system->SetBlendMode(static_cast<BlendMode>(json["BlendMode"].get<int>()));
	if (json.contains("Lighting"))
		system->SetLightSetting(json["Lighting"].get<bool>() ? ParticleLightSetting{ true, true, true } : ParticleLightSetting{ false, false, false });
	if (json.contains("softParticle"))     system->SetSoftParticle(json["softParticle"].get<bool>());
	if (json.contains("softFadeDistance")) system->SetSoftFadeDistance(json["softFadeDistance"].get<float>());
	if (json.contains("emissiveIntensity")) system->SetEmissiveIntensity(json["emissiveIntensity"].get<float>());

	if (json.contains("mesh")) {
		const auto& meshJson = json["mesh"];
		if (meshJson.contains("type") && meshJson.contains("params"))
			system->SetMeshType(meshJson["type"], meshJson["params"]);
	}

	if (json.contains("spawnModules")) {
		for (const auto& mj : json["spawnModules"]) {
			if (!mj.contains("type")) continue;
			auto module = YParticleModuleFactory::GetInstance().CreateSpawnModule(mj["type"]);
			if (module) {
				if (mj.contains("data")) module->LoadFromJson(mj["data"]);
				system->AddSpawnModule(module);
			}
		}
	}

	if (json.contains("updateModules")) {
		for (const auto& mj : json["updateModules"]) {
			if (!mj.contains("type")) continue;
			auto module = YParticleModuleFactory::GetInstance().CreateUpdateModule(mj["type"]);
			if (module) {
				if (mj.contains("data")) module->LoadFromJson(mj["data"]);
				system->AddUpdateModule(module);
			}
		}
	}
}

//=================================================================
// 内部UI関数
//=================================================================

void YParticleEditor::ShowSystemCreationUI() {
	ImGui::InputText("システムの名前", newSystemNameBuffer_, sizeof(newSystemNameBuffer_));
	ImGui::InputInt("パーティクルの最大数", &newSystemMaxParticles_);
	if (newSystemMaxParticles_ < 1) newSystemMaxParticles_ = 1;

	ImGui::Separator();

	if (ImGui::Button("システムの生成", ImVec2(200, 30))) {
		if (strlen(newSystemNameBuffer_) > 0) {
			YParticleManager::GetInstance().CreateSystem(newSystemNameBuffer_, newSystemMaxParticles_);
			selectedSystemName_ = newSystemNameBuffer_;
			newSystemNameBuffer_[0] = '\0';
			newSystemMaxParticles_ = 1000;
		}
	}
}

void YParticleEditor::ShowSystemListUI() {
	ImGui::Text("システム一覧");
	ImGui::Separator();

	auto& manager = YParticleManager::GetInstance();
	for (const auto& name : manager.GetAllSystemNames()) {
		bool isSelected = (name == selectedSystemName_);
		if (ImGui::Selectable(name.c_str(), isSelected)) {
			selectedSystemName_ = name;
		}
	}
}

void YParticleEditor::ShowSystemDetailUI() {
	if (selectedSystemName_.empty()) {
		ImGui::Text("パーティクルシステムが選択されていません");
		return;
	}
	auto& manager = YParticleManager::GetInstance();
	auto* system = manager.GetSystem(selectedSystemName_);
	if (!system) {
		ImGui::Text("パーティクルシステムが見つかりません: %s", selectedSystemName_.c_str());
		return;
	}
	system->ShowEditor();
}

//=================================================================
// ファイル操作タブ（FileBrowser 版）
//=================================================================

/// ポップアップ用のヘルパー：ブラウザ + 新規名入力 + キャンセル
static void DrawBrowserPopup(
	const char* popupId,
	bool& showFlag,
	const char* title,
	YoRigine::FileBrowser& browser,
	char* saveAsNameBuf,
	size_t saveAsNameBufSize,
	std::function<void(const std::string&)> onSaveAs   // nullptr なら「上書き専用」（Load 用）
) {
	if (showFlag) ImGui::OpenPopup(popupId);

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(popupId, &showFlag,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
		ImGui::Text("%s", title);
		ImGui::Separator();

		float browserH = onSaveAs ? 270.0f : 310.0f;
		browser.Draw("##BrowserChild", ImVec2(0, browserH));

		ImGui::Separator();

		// 新規ファイル名入力（Save 系のみ）
		if (onSaveAs) {
			ImGui::SetNextItemWidth(-90);
			ImGui::InputTextWithHint("##SaveAs", "新規ファイル.json", saveAsNameBuf, (int)saveAsNameBufSize);
			ImGui::SameLine();
			if (ImGui::Button("名前を付けて保存")) {
				if (saveAsNameBuf[0] != '\0') {
					onSaveAs(browser.GetCurrentDir() + saveAsNameBuf);
					saveAsNameBuf[0] = '\0';
					showFlag = false;
					ImGui::CloseCurrentPopup();
				}
			}
		}

		if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
			showFlag = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void YParticleEditor::ShowFileOperationsUI() {
	ImGui::Text("ファイル操作");
	ImGui::Separator();

	// ── Save All ────────────────────────────────────────────
	if (ImGui::Button("すべてのシステムを保存", ImVec2(220, 30))) {
		saveAllBrowser_.Scan();
		saveAsNameBuf_[0] = '\0';   // 全保存はデフォルト名なし
		showSaveAllPopup_ = true;
	}
	DrawBrowserPopup(
		"##SaveAll", showSaveAllPopup_,
		"すべてのシステムを保存 — クリックで上書き、または新しい名前を入力",
		saveAllBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_),
		[this](const std::string& path) {
			bool ok = SaveAllSystemsToFile(path);
			SetNotification(ok, ok ? "全システム保存完了: " + path : "保存失敗: " + path);
		}
	);

	// ── Load All ────────────────────────────────────────────
	if (ImGui::Button("すべてのシステムを読み込み", ImVec2(220, 30))) {
		loadAllBrowser_.Scan();
		showLoadAllPopup_ = true;
	}
	DrawBrowserPopup(
		"##LoadAll", showLoadAllPopup_,
		"すべてのシステムを読み込み — ファイルをクリックして読み込む",
		loadAllBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_),
		nullptr // Load なので新規名入力欄は不要
	);

	ImGui::Separator();

	// ── Save Selected ───────────────────────────────────────
	ImGui::BeginDisabled(selectedSystemName_.empty());
	if (!selectedSystemName_.empty()) {
		ImGui::Text("選択中: %s", selectedSystemName_.c_str());
	}
	if (ImGui::Button("選択したシステムを保存", ImVec2(220, 30))) {
		saveSingleBrowser_.Scan();
		// ★ テンプレート名: 選択中のシステム名 + ".json"
		std::string defaultName = selectedSystemName_ + ".json";
		strncpy_s(saveAsNameBuf_, defaultName.c_str(), sizeof(saveAsNameBuf_));
		showSaveSinglePopup_ = true;
	}
	ImGui::EndDisabled();

	DrawBrowserPopup(
		"##SaveSingle", showSaveSinglePopup_,
		"選択したシステムを保存 — クリックで上書き、または新しい名前を入力",
		saveSingleBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_),
		[this](const std::string& path) {
			if (!selectedSystemName_.empty()) {
				bool ok = SaveSystemToFile(selectedSystemName_, path);
				SetNotification(ok, ok ? "保存完了: " + path : "保存失敗: " + path);
			}
		}
	);

	// ── Load Single ─────────────────────────────────────────
	if (ImGui::Button("ファイルからシステムを読み込み", ImVec2(220, 30))) {
		loadSingleBrowser_.Scan();
		showLoadSinglePopup_ = true;
	}
	DrawBrowserPopup(
		"##LoadSingle", showLoadSinglePopup_,
		"システムを読み込み — ファイルをクリックして読み込む",
		loadSingleBrowser_, saveAsNameBuf_, sizeof(saveAsNameBuf_),
		nullptr
	);
}
#endif // USE_IMGUI