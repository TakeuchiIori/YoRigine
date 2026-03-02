#include "ParticleEditor.h"
#include "ParticleManager.h"
#include "ParticleSystem.h"
#include <algorithm>

/// <summary>
/// エディターの初期化処理を行う
/// JSONマネージャの初期設定と ImGui Helper の生成を行う
/// </summary>
void ParticleEditor::Initialize() {
	jsonManager_ = &ParticleJsonManager::GetInstance();
	jsonManager_->SetBaseDirectory("Resources/Json/Particles/");

	imguiHelper_ = std::make_unique<ParticleImGuiHelper>(nullptr);
	imguiHelper_->SetChangeCallback([this]() {
		OnSettingsChanged();
		});
}

/// <summary>
/// パーティクルエディターウィンドウを表示する
/// システム選択やタブ表示など UI の入口となる
/// </summary>
void ParticleEditor::ShowEditor() {
#ifdef USE_IMGUI
	if (!isOpen_) return;

	ImGui::SetNextWindowSize(ImVec2(500, 800), ImGuiCond_FirstUseEver);
	ShowMenuBar();
	ShowSystemSelector();

	if (currentSystem_) {
		ImGui::Separator();
		ShowTabBar();
	}

	UpdateNotifications();
#endif
}

/// <summary>
/// メニューバーの描画
/// 保存・読み込み・プリセットなど上部メニューを構築する
/// </summary>
void ParticleEditor::ShowMenuBar() {
#ifdef USE_IMGUI
	if (ImGui::BeginMenuBar()) {

		if (ImGui::BeginMenu("ファイル")) {
			if (ImGui::MenuItem("保存", "Ctrl+S", false, currentSystem_ != nullptr)) {
				SaveCurrentSystem();
			}
			if (ImGui::MenuItem("読み込み", "Ctrl+O", false, currentSystem_ != nullptr)) {
				LoadCurrentSystem();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("全システム保存")) {
				// 全システム保存処理
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("プリセット")) {
			if (ImGui::MenuItem("炎", nullptr, false, currentSystem_ != nullptr)) imguiHelper_->ApplyFirePreset();
			if (ImGui::MenuItem("煙", nullptr, false, currentSystem_ != nullptr)) imguiHelper_->ApplySmokePreset();
			if (ImGui::MenuItem("魔法", nullptr, false, currentSystem_ != nullptr)) imguiHelper_->ApplyMagicPreset();
			if (ImGui::MenuItem("爆発", nullptr, false, currentSystem_ != nullptr)) imguiHelper_->ApplyExplosionPreset();
			if (ImGui::MenuItem("雨", nullptr, false, currentSystem_ != nullptr)) imguiHelper_->ApplyRainPreset();
			if (ImGui::MenuItem("雪", nullptr, false, currentSystem_ != nullptr)) imguiHelper_->ApplySnowPreset();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("表示")) {
			ImGui::Text("タブ切り替え:");
			if (ImGui::MenuItem("プロパティ", "1")) selectedTab_ = TAB_PROPERTIES;
			if (ImGui::MenuItem("プレビュー", "2")) selectedTab_ = TAB_PREVIEW;
			if (ImGui::MenuItem("ファイル", "3")) selectedTab_ = TAB_FILES;
			if (ImGui::MenuItem("パフォーマンス", "4")) selectedTab_ = TAB_PERFORMANCE;
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
#endif
}

/// <summary>
/// 使用するパーティクルシステムの選択 UI を描画する
/// コンボボックスからシステムを切り替える
/// </summary>
void ParticleEditor::ShowSystemSelector() {
#ifdef USE_IMGUI
	ImGui::Text("パーティクルシステム:");
	ImGui::SameLine();

	auto* particleManager = YoRigine::ParticleManager::GetInstance();
	auto systemNames = particleManager->GetAllSystemNames();

	const char* currentItemText = selectedSystem_.empty() ? "システムを選択..." : selectedSystem_.c_str();

	if (ImGui::BeginCombo("##SystemSelector", currentItemText)) {
		for (const auto& name : systemNames) {
			bool isSelected = (selectedSystem_ == name);
			if (ImGui::Selectable(name.c_str(), isSelected)) {
				selectedSystem_ = name;
				UpdateCurrentSystem();
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::Text("(%zu個)", systemNames.size());

	if (ImGui::Button("更新")) {
		UpdateCurrentSystem();
	}
#endif
}

/// <summary>
/// タブバーを表示し、各タブの描画関数を呼び出す
/// </summary>
void ParticleEditor::ShowTabBar() {
#ifdef USE_IMGUI
	if (ImGui::BeginTabBar("ParticleEditorTabs")) {

		if (ImGui::BeginTabItem("プロパティ")) {
			selectedTab_ = TAB_PROPERTIES;
			ShowPropertiesTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("プレビュー")) {
			selectedTab_ = TAB_PREVIEW;
			ShowPreviewTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ファイル")) {
			selectedTab_ = TAB_FILES;
			ShowFilesTab();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("パフォーマンス")) {
			selectedTab_ = TAB_PERFORMANCE;
			ShowPerformanceTab();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
#endif
}

/// <summary>
/// プロパティタブの描画
/// ParticleImGuiHelper を使って全ての設定項目を表示する
/// </summary>
void ParticleEditor::ShowPropertiesTab() {
	if (imguiHelper_) {
		imguiHelper_->ShowAllSettings();
	}
}

/// <summary>
/// プレビュータブの描画
/// プレビュー再生やリアルタイム情報を表示する
/// </summary>
void ParticleEditor::ShowPreviewTab() {
#ifdef USE_IMGUI
	ImGui::Text("プレビュー制御");

	ImGui::Checkbox("プレビュー有効", &enablePreview_);

	if (enablePreview_) {
		// プレビュー操作系 UI
		ShowPreviewControls();

		ImGui::Separator();
		ImGui::Text("リアルタイム情報");
		ImGui::Text("アクティブパーティクル: %zu", currentSystem_->GetParticleCount());
		ImGui::Text("システム状態: %s", currentSystem_->IsActive() ? "動作中" : "停止中");

		// パーティクル利用率表示
		float utilization =
			(float)currentSystem_->GetParticleCount() /
			(float)currentSystem_->GetSettings().GetMaxParticles() * 100.0f;
		ImGui::Text("利用率: %.1f%%", utilization);

		if (utilization > 90.0f) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			ImGui::Text("警告: パーティクル利用率が高いです！");
			ImGui::PopStyleColor();
		}
	}
#endif
}

/// <summary>
/// ファイルタブの描画
/// 設定の保存・読込、プリセット操作をまとめて表示する
/// </summary>
void ParticleEditor::ShowFilesTab() {
#ifdef USE_IMGUI
	ShowFileOperations();
	ImGui::Separator();
	ShowPresetManager();
#endif
}

/// <summary>
/// パフォーマンスタブの描画
/// パーティクルシステムの負荷情報を表示する
/// </summary>
void ParticleEditor::ShowPerformanceTab() {
#ifdef USE_IMGUI

	auto perfInfo = YoRigine::ParticleManager::GetInstance()->GetPerformanceInfo();

	ImGui::Text("パフォーマンス情報");
	ImGui::Separator();

	ImGui::Text("総パーティクル数: %d", perfInfo.totalParticles);
	ImGui::Text("アクティブグループ数: %d", perfInfo.activeGroups);
	ImGui::Text("更新時間: %.3f ms", perfInfo.updateTime);
	ImGui::Text("描画時間: %.3f ms", perfInfo.renderTime);
	ImGui::Text("総フレーム時間: %.3f ms",
		perfInfo.updateTime + perfInfo.renderTime);

	if (currentSystem_) {
		ImGui::Separator();
		ImGui::Text("現在のシステム");
		ImGui::Text("パーティクル数: %zu", currentSystem_->GetParticleCount());
		ImGui::Text("最大パーティクル数: %d",
			currentSystem_->GetSettings().GetMaxParticles());
		ImGui::Text("エミッション率: %.1f/秒",
			currentSystem_->GetSettings().GetEmissionRate());

		float memoryUsage =
			currentSystem_->GetParticleCount() * sizeof(ParticleData) /
			1024.0f / 1024.0f;
		ImGui::Text("メモリ使用量: %.2f MB", memoryUsage);
	}

	if (ImGui::Button("パフォーマンスカウンターリセット")) {
		// カウンターリセット処理（必要に応じて実装）
	}
#endif
}

/// <summary>
/// 設定ファイルの保存/読込 UI を描画する
/// 現在のパーティクル設定の保存や既存設定の選択ができる
/// </summary>
void ParticleEditor::ShowFileOperations() {
#ifdef USE_IMGUI

	ImGui::Text("ファイル操作");

	if (ImGui::Button("設定を保存", ImVec2(-1, 0))) {
		SaveCurrentSystem();
	}

	if (ImGui::Button("設定を読み込み", ImVec2(-1, 0))) {
		LoadCurrentSystem();
	}

	ImGui::Separator();

	// 既に保存されている設定一覧を表示
	auto availableSettings = jsonManager_->GetAvailableSettings();
	if (!availableSettings.empty()) {
		ImGui::Text("利用可能な設定:");
		for (const auto& setting : availableSettings) {
			if (ImGui::Selectable(setting.c_str())) {
				selectedSystem_ = setting;
				UpdateCurrentSystem();
			}
		}
	} else {
		ImGui::Text("保存された設定がありません");
	}
#endif
}

/// <summary>
/// プリセットの保存・読み込み・削除を行う管理 UI を描画する
/// </summary>
void ParticleEditor::ShowPresetManager() {
#ifdef USE_IMGUI

	ImGui::Text("プリセット管理");

	// 新しいプリセットを保存
	ImGui::InputText("プリセット名", newPresetName_, sizeof(newPresetName_));
	ImGui::SameLine();
	if (ImGui::Button("保存") && strlen(newPresetName_) > 0) {
		SavePreset(std::string(newPresetName_));
		newPresetName_[0] = '\0';
	}

	ImGui::Separator();

	auto availablePresets = jsonManager_->GetAvailablePresets();
	if (!availablePresets.empty()) {
		ImGui::Text("利用可能なプリセット:");
		for (const auto& preset : availablePresets) {
			ImGui::PushID(preset.c_str());

			if (ImGui::Button(("読み込み##" + preset).c_str())) {
				LoadPreset(preset);
			}
			ImGui::SameLine();
			if (ImGui::Button(("削除##" + preset).c_str())) {
				jsonManager_->DeletePreset(preset);
				AddNotification("プリセットを削除しました: " + preset);
			}
			ImGui::SameLine();
			ImGui::Text("%s", preset.c_str());

			ImGui::PopID();
		}
	} else {
		ImGui::Text("保存されたプリセットがありません");
	}
#endif
}

/// <summary>
/// プレビュータブ内の制御系 UI を描画する
/// エミッション、開始/停止、簡易プリセットなどをまとめて操作できる
/// </summary>
void ParticleEditor::ShowPreviewControls() {
#ifdef USE_IMGUI

	// プレビュー位置調整
	ImGui::SliderFloat3("プレビュー位置", &previewPosition_.x, -10.0f, 10.0f);

	// ────────────────────────────────
	// パーティクル発生ボタン
	// ────────────────────────────────
	if (ImGui::Button("1個発生", ImVec2(80, 0))) {
		EmitParticles(1);
	}
	ImGui::SameLine();
	if (ImGui::Button("10個発生", ImVec2(80, 0))) {
		EmitParticles(10);
	}
	ImGui::SameLine();
	if (ImGui::Button("バースト", ImVec2(80, 0))) {
		EmitParticles(100);
	}

	// ────────────────────────────────
	// システム制御
	// ────────────────────────────────
	if (ImGui::Button("開始", ImVec2(80, 0))) {
		currentSystem_->SetActive(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("停止", ImVec2(80, 0))) {
		currentSystem_->SetActive(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("リセット", ImVec2(80, 0))) {
		ResetSystem();
	}

	// ────────────────────────────────
	// クイックプリセット
	// ────────────────────────────────
	ImGui::Separator();
	ImGui::Text("クイックプリセット:");

	if (ImGui::Button("炎")) imguiHelper_->ApplyFirePreset();
	ImGui::SameLine();
	if (ImGui::Button("煙")) imguiHelper_->ApplySmokePreset();
	ImGui::SameLine();
	if (ImGui::Button("魔法")) imguiHelper_->ApplyMagicPreset();

	if (ImGui::Button("爆発")) imguiHelper_->ApplyExplosionPreset();
	ImGui::SameLine();
	if (ImGui::Button("雨")) imguiHelper_->ApplyRainPreset();
	ImGui::SameLine();
	if (ImGui::Button("雪")) imguiHelper_->ApplySnowPreset();
#endif
}

/// <summary>
/// 現在選択中のパーティクルシステムの設定を保存する
/// </summary>
void ParticleEditor::SaveCurrentSystem() {
	if (!currentSystem_) return;

	if (jsonManager_->SaveSettings(selectedSystem_, currentSystem_->GetSettings())) {
		AddNotification("設定を保存しました: " + selectedSystem_);
	} else {
		AddNotification("設定の保存に失敗しました", true);
	}
}

/// <summary>
/// 現在選択中のパーティクルシステムの設定を読み込む
/// </summary>
void ParticleEditor::LoadCurrentSystem() {
	if (!currentSystem_) return;

	auto& settings = currentSystem_->GetSettings();
	if (jsonManager_->LoadSettings(selectedSystem_, settings)) {
		AddNotification("設定を読み込みました: " + selectedSystem_);
	} else {
		AddNotification("設定の読み込みに失敗しました", true);
	}
}

/// <summary>
/// 全てのパーティクルシステムに対して設定ファイルを一括ロードする
/// Settingsフォルダ内のJSONを全システムに適用する
/// </summary>
void ParticleEditor::LoadAllSystems()
{
	auto availableSettings = jsonManager_->GetAvailableSettings();

	for (const std::string& settingName : availableSettings) {
		auto* system = YoRigine::ParticleManager::GetInstance()->GetSystem(settingName);
		if (system) {
			auto& settings = system->GetSettings();
			if (jsonManager_->LoadSettings(settingName, settings)) {
				AddNotification("設定読み込み: " + settingName);
			}
		}
	}
}

/// <summary>
/// 現在の設定をプリセットとして保存する
/// </summary>
void ParticleEditor::SavePreset(const std::string& name) {
	if (!currentSystem_) return;

	if (jsonManager_->SavePreset(name, currentSystem_->GetSettings())) {
		AddNotification("プリセットを保存しました: " + name);
	} else {
		AddNotification("プリセットの保存に失敗しました", true);
	}
}

/// <summary>
/// 指定したプリセット名の設定をロードする
/// </summary>
void ParticleEditor::LoadPreset(const std::string& name) {
	if (!currentSystem_) return;

	auto& settings = currentSystem_->GetSettings();
	if (jsonManager_->LoadPreset(name, settings)) {
		AddNotification("プリセットを読み込みました: " + name);
	} else {
		AddNotification("プリセットの読み込みに失敗しました", true);
	}
}

/// <summary>
/// 指定数だけパーティクルを発生させる
/// </summary>
void ParticleEditor::EmitParticles(int count) {
	if (currentSystem_) {
		currentSystem_->Emit(previewPosition_, count);
	}
}

/// <summary>
/// パーティクルシステムをリセットする
/// （停止 + 将来的には「全パーティクル消去」処理を追加すると良い）
/// </summary>
void ParticleEditor::ResetSystem() {
	if (currentSystem_) {
		currentSystem_->SetActive(false);
		// ※ 全パーティクル削除は ParticleSystem に Clear() が必要
	}
}

/// <summary>
/// システム名を登録し、未選択ならそのシステムを選択状態にする
/// </summary>
void ParticleEditor::RegisterSystem(const std::string& name) {
	if (selectedSystem_.empty()) {
		selectedSystem_ = name;
		UpdateCurrentSystem();
	}
}

/// <summary>
/// 現在の選択中システムを更新し、ImGuiHelper に設定を反映する
/// </summary>
void ParticleEditor::UpdateCurrentSystem() {
	currentSystem_ = nullptr;

	if (!selectedSystem_.empty()) {
		currentSystem_ = YoRigine::ParticleManager::GetInstance()->GetSystem(selectedSystem_);

		if (currentSystem_ && imguiHelper_) {
			imguiHelper_ = std::make_unique<ParticleImGuiHelper>(&currentSystem_->GetSettings());
			imguiHelper_->SetChangeCallback([this]() { OnSettingsChanged(); });
		}
	}
}

/// <summary>
/// 設定変更時のコールバック
/// 今後 Undo / Redo や自動保存を実装する場合にここを拡張する
/// </summary>
void ParticleEditor::OnSettingsChanged() {
	// 設定が変更されたときの処理（今後の拡張ポイント）
}

/// <summary>
/// ツール内で右上に出る通知を追加する
/// </summary>
void ParticleEditor::AddNotification(const std::string& message, bool isError) {
	notifications_.push_back({ message, isError, 3.0f }); // 3秒表示
}

/// <summary>
/// 通知の更新と描画を行う
/// 時間でフェードアウトし、消えたものを削除する
/// </summary>
void ParticleEditor::UpdateNotifications() {
#ifdef USE_IMGUI

	float deltaTime = ImGui::GetIO().DeltaTime;

	// 時間経過
	for (auto& n : notifications_) n.timeLeft -= deltaTime;

	// 消えた通知を除去
	notifications_.erase(
		std::remove_if(notifications_.begin(), notifications_.end(),
			[](const Notification& n) { return n.timeLeft <= 0.0f; }),
		notifications_.end());

	// 表示
	if (!notifications_.empty()) {
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 320, 30), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);

		if (ImGui::Begin("通知", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
		{
			for (const auto& n : notifications_) {
				if (n.isError) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
					ImGui::Text("❌ %s", n.message.c_str());
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
					ImGui::Text("✅ %s", n.message.c_str());
					ImGui::PopStyleColor();
				}
			}
		}
		ImGui::End();
	}
#endif
}
