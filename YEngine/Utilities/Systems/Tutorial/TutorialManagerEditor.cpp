#include "TutorialManager.h"

#ifdef USE_IMGUI

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#include <imgui.h>
#include "Editor/Widgets/YEditorWidget.h"
#include "Systems/UI/UIManager.h"

namespace {
	// 条件ツリーの入れ子の深さ上限。UIが際限なく深くならないよう抑える。
	constexpr int kMaxConditionDepth = 4;

	// 完了条件ツリーの編集UI。all/any/not は子を持つため再帰的に描く。
	bool DrawConditionEditor(YoRigine::TutorialCondition& condition,
		const std::vector<std::string>& signalNames, int depth) {
		using YoRigine::TutorialCondition;
		using YoRigine::TutorialConditionType;

		// 並び順は TutorialConditionType の宣言順と一致させること。
		static constexpr std::string_view kTypeNames[] = {
			"使わない（下の旧設定に従う）",
			"シグナルを待つ",
			"時間が経過する",
			"決定入力を待つ",
			"すべて成立したら",
			"いずれか成立したら",
			"成立しなければ",
		};

		bool changed = YEditorWidget::EnumCombo("条件の種類", condition.type, kTypeNames);

		switch (condition.type) {
		case TutorialConditionType::Signal:
			if (!signalNames.empty()) {
				changed |= YEditorWidget::StringCombo("シグナル名", condition.signalName, signalNames, true);
			}
			changed |= YEditorWidget::InputText("シグナル名を直接入力", condition.signalName);
			YEditorWidget::HelpMarker(
				"入力アクションは action.triggered.<アクション名> という名前で自動的に流れてきます。"
				"ゲーム固有の出来事は TutorialSignal::Emit(\"名前\") をゲーム側から呼んでください");
			changed |= YEditorWidget::DragInt("必要な回数", condition.requiredCount, 1.0f, 1, 99);
			break;

		case TutorialConditionType::Elapsed:
			changed |= YEditorWidget::DragFloat("経過秒数", condition.seconds, 0.1f, 0.0f, 600.0f, "%.1f");
			break;

		case TutorialConditionType::All:
		case TutorialConditionType::Any:
		case TutorialConditionType::Not: {
			// 「成立しなければ」は先頭の子だけを見るため、子は1つで足りる。
			const bool acceptsMore = (condition.type != TutorialConditionType::Not)
				|| condition.children.empty();

			ImGui::Indent();
			if (acceptsMore && ImGui::SmallButton("子の条件を追加")) {
				condition.children.push_back(TutorialCondition{});
				changed = true;
			}

			int removeIndex = -1;
			for (int i = 0; i < static_cast<int>(condition.children.size()); ++i) {
				ImGui::PushID(i);
				const std::string header = "条件 " + std::to_string(i + 1);
				if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
					if (depth < kMaxConditionDepth) {
						changed |= DrawConditionEditor(condition.children[i], signalNames, depth + 1);
					}
					else {
						ImGui::TextDisabled("これ以上は入れ子にできません");
					}
					if (ImGui::SmallButton("この条件を削除")) removeIndex = i;
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			if (removeIndex >= 0) {
				condition.children.erase(condition.children.begin() + removeIndex);
				changed = true;
			}
			ImGui::Unindent();
			break;
		}

		case TutorialConditionType::None:
		case TutorialConditionType::Confirm:
		default:
			break;
		}
		return changed;
	}

	// 暗幕（スポットライト）の編集UI。
	bool DrawSpotlightEditor(YoRigine::TutorialSpotlightConfig& spotlight,
		const std::vector<std::string>& uiIds) {
		using YoRigine::TutorialSpotlightTarget;
		using YoRigine::TutorialSpotlightTargetKind;

		bool changed = YEditorWidget::Checkbox("注目させたい場所以外を暗くする", spotlight.enabled);
		YEditorWidget::HelpMarker(
			"指定した場所だけ穴を開けて、それ以外を暗幕で覆います。"
			"穴の中は下にあるUIも3Dの画もそのまま明るく残ります");
		if (!spotlight.enabled) return changed;

		changed |= YEditorWidget::Color("暗幕の色", spotlight.dimColor);
		changed |= YEditorWidget::DragFloat("穴の余白(px)", spotlight.padding, 1.0f, 0.0f, 200.0f, "%.0f");
		changed |= YEditorWidget::DragFloat("暗転にかける時間(秒)", spotlight.fadeSeconds, 0.05f, 0.0f, 3.0f, "%.2f");

		// 並び順は TutorialSpotlightTargetKind の宣言順と一致させること。
		static constexpr std::string_view kKindNames[] = {
			"UIを指定", "画面上の矩形を指定", "ワールド上の対象を指定",
		};

		if (ImGui::Button("注目させる場所を追加")) {
			spotlight.targets.push_back(TutorialSpotlightTarget{});
			changed = true;
		}

		const std::vector<std::string> worldNames =
			YoRigine::TutorialSpotlight::GetInstance()->GetWorldTargetNames();

		int removeIndex = -1;
		for (int i = 0; i < static_cast<int>(spotlight.targets.size()); ++i) {
			TutorialSpotlightTarget& target = spotlight.targets[i];
			ImGui::PushID(i);
			const std::string header = std::to_string(i + 1) + ". " +
				(target.id.empty() ? std::string("(未設定)") : target.id);
			if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				changed |= YEditorWidget::EnumCombo("対象の種類", target.kind, kKindNames);

				switch (target.kind) {
				case TutorialSpotlightTargetKind::Ui:
					if (!uiIds.empty()) {
						changed |= YEditorWidget::StringCombo("UI", target.id, uiIds, true);
					}
					else {
						changed |= YEditorWidget::InputText("UI ID", target.id);
					}
					break;

				case TutorialSpotlightTargetKind::Rect:
					changed |= YEditorWidget::DragVec2("中心位置", target.center, 1.0f, -2048.0f, 4096.0f);
					changed |= YEditorWidget::DragVec2("大きさ", target.size, 1.0f, 1.0f, 4096.0f);
					break;

				case TutorialSpotlightTargetKind::World:
					if (!worldNames.empty()) {
						changed |= YEditorWidget::StringCombo("登録名", target.id, worldNames, true);
					}
					else {
						changed |= YEditorWidget::InputText("登録名", target.id);
						ImGui::TextDisabled("ゲーム側から RegisterWorldTarget で登録された名前がここに並びます");
					}
					changed |= YEditorWidget::DragFloat("半径(ワールド単位)", target.radius, 0.1f, 0.1f, 100.0f, "%.1f");
					break;

				default:
					break;
				}

				if (ImGui::SmallButton("この対象を削除")) removeIndex = i;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if (removeIndex >= 0) {
			spotlight.targets.erase(spotlight.targets.begin() + removeIndex);
			changed = true;
		}
		return changed;
	}

	std::vector<std::string> ListTutorialFiles() {
		std::vector<std::string> files;
		std::error_code error;
		const std::filesystem::path directory("Resources/Json/Tutorials");
		if (!std::filesystem::exists(directory, error)) return files;
		for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				files.push_back(entry.path().generic_string());
			}
		}
		std::sort(files.begin(), files.end());
		return files;
	}

	std::vector<std::string> ListFontFiles() {
		std::vector<std::string> files;
		std::error_code error;
		const std::filesystem::path directory("Resources/Fonts");
		if (!std::filesystem::exists(directory, error)) return files;
		for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
			if (!entry.is_regular_file()) continue;
			std::string extension = entry.path().extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (extension == ".ttf" || extension == ".otf") {
				files.push_back(entry.path().generic_string());
			}
		}
		std::sort(files.begin(), files.end());
		return files;
	}

	std::vector<std::string> ListUIIds() {
		std::vector<std::string> ids;
		for (const auto& [id, ui] : YoRigine::UIManager::GetInstance()->GetAllUIs()) {
			if (ui && !ui->IsTransient()) ids.push_back(id);
		}
		std::sort(ids.begin(), ids.end());
		return ids;
	}

	std::vector<std::string> ListPanelTextures() {
		std::vector<std::string> files;
		const std::filesystem::path roots[] = { "Resources/UITex", "Resources/images" };
		for (const auto& root : roots) {
			std::error_code error;
			if (!std::filesystem::exists(root, error)) continue;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
				if (!entry.is_regular_file()) continue;
				std::string extension = entry.path().extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
					const std::string path = entry.path().generic_string();
					if (path.find("__tutorial_runtime") == std::string::npos &&
						path.find("__bake_preview") == std::string::npos) files.push_back(path);
				}
			}
		}
		std::sort(files.begin(), files.end());
		files.erase(std::unique(files.begin(), files.end()), files.end());
		return files;
	}
}

namespace YoRigine {

	void TutorialManager::DrawEditor() {
		if (editorPreviewPending_ && ImGui::GetTime() - editorPreviewChangeTime_ >= 0.12) {
			runtimeUIDirty_ = true;
			editorPreviewPending_ = false;
		}
		static std::vector<std::string> availableFiles;
		static std::vector<std::string> availableFonts;
		static std::vector<std::string> availablePanelTextures;
		static const std::vector<std::string> keyboardKeys{ "SPACE", "ENTER", "E", "F", "TAB", "ESC" };
		static const std::vector<std::string> gamepadButtons{ "A", "B", "X", "Y", "START", "BACK" };
		static int availableSelection = 0;
		static bool resourcesScanned = false;
		static bool initialFileLoaded = false;
		if (!resourcesScanned) {
			availableFiles = ListTutorialFiles();
			availableFonts = ListFontFiles();
			availablePanelTextures = ListPanelTextures();
			for (const std::string& path : availableFiles) {
				TutorialData discovered;
				Load(discovered, path); // 保存済みデータ内のイベント名も候補へ登録
			}
			resourcesScanned = true;
		}
		if (!initialFileLoaded) {
			TutorialData loaded;
			if (Load(loaded, editorPath_)) {
				editorData_ = std::move(loaded);
				editorSelectedStep_ = 0;
				editorStatus_ = "起動時に読み込みました: " + editorPath_;
			}
			initialFileLoaded = true;
		}
		const std::vector<std::string> uiIds = ListUIIds();

		ImGui::TextUnformatted("説明ステップを並べて、ゲーム内でそのまま再生できます");
		ImGui::TextDisabled("待機方法: 決定入力 / 指定秒数 / ゲームイベント");

		if (YEditorWidget::Section section{ "ファイル" }) {
			if (!availableFiles.empty()) {
				availableSelection = std::clamp(availableSelection, 0, static_cast<int>(availableFiles.size()) - 1);
				if (YEditorWidget::StringCombo("既存チュートリアル", editorPath_, availableFiles)) {
					auto it = std::find(availableFiles.begin(), availableFiles.end(), editorPath_);
					if (it != availableFiles.end()) availableSelection = static_cast<int>(std::distance(availableFiles.begin(), it));
					TutorialData loaded;
					if (Load(loaded, editorPath_)) {
						editorData_ = std::move(loaded);
						editorSelectedStep_ = 0;
						editorStatus_ = "選択したファイルを読み込みました: " + editorPath_;
					}
					else {
						editorStatus_ = "選択したファイルの読み込みに失敗しました";
					}
				}
			}
			if (YEditorWidget::TreeNode directPath{ "保存先を直接編集" }) {
				YEditorWidget::InputText("保存先", editorPath_);
			}
			if (ImGui::Button("新規作成")) {
				editorData_ = TutorialData{};
				editorData_.steps.push_back(TutorialStep{});
				editorSelectedStep_ = 0;
				editorStatus_ = "新しいチュートリアルを作成しました";
			}
			ImGui::SameLine();
			if (ImGui::Button("保存")) {
				editorStatus_ = Save(editorData_, editorPath_) ? "保存しました: " + editorPath_ : "保存に失敗しました";
				availableFiles = ListTutorialFiles();
			}
			ImGui::SameLine();
			if (ImGui::Button("読込")) {
				TutorialData loaded;
				if (Load(loaded, editorPath_)) {
					editorData_ = std::move(loaded);
					editorSelectedStep_ = 0;
					editorStatus_ = "読み込みました: " + editorPath_;
				}
				else editorStatus_ = "読み込みに失敗しました";
			}

			if (ImGui::Button("一覧を更新")) {
				availableFiles = ListTutorialFiles();
				availableFonts = ListFontFiles();
				availablePanelTextures = ListPanelTextures();
			}
			if (!editorStatus_.empty()) ImGui::TextWrapped("%s", editorStatus_.c_str());
		}

		if (YEditorWidget::Section section{ "再生確認" }) {
			if (!IsPlaying()) {
				if (ImGui::Button("最初から再生") && !editorData_.steps.empty()) Start(editorData_);
				ImGui::SameLine();
				if (ImGui::Button("選択ステップから再生") && !editorData_.steps.empty()) {
					Start(editorData_, static_cast<std::size_t>(std::max(0, editorSelectedStep_)));
				}
			}
			else {
				ImGui::Text("再生中: %zu / %zu", currentStep_ + 1, currentData_.steps.size());
				if (ImGui::Button("次へ")) Advance();
				ImGui::SameLine();
				if (ImGui::Button("停止")) Stop();
				if (currentStep_ < currentData_.steps.size() &&
					currentData_.steps[currentStep_].waitType == TutorialWaitType::Event) {
					const std::string& eventName = currentData_.steps[currentStep_].eventName;
					if (ImGui::Button("現在のイベントを送信")) NotifyEvent(eventName);
					ImGui::SameLine();
					ImGui::TextDisabled("%s", eventName.c_str());
				}
			}
		}

		bool styleChanged = false;
		if (YEditorWidget::Section section{ "全ページ共通のデザイン" }) {
			YEditorWidget::Checkbox("再生中にリアルタイム反映", editorLivePreview_);
			ImGui::TextDisabled("位置とサイズは、各ページの編集欄で個別に設定します");

			YEditorWidget::SectionHeader("説明パネル");
			styleChanged |= YEditorWidget::StringCombo("説明パネル画像", editorData_.style.panelTexturePath,
				availablePanelTextures, true);
			styleChanged |= YEditorWidget::Color("説明パネル色", editorData_.style.panelColor);

			YEditorWidget::SectionHeader("説明文のデザイン");
			styleChanged |= YEditorWidget::InputText("チュートリアル名", editorData_.name);
			styleChanged |= YEditorWidget::StringCombo("フォント", editorData_.style.fontFilePath, availableFonts, true);
			if (editorData_.style.fontFilePath.empty()) {
				styleChanged |= YEditorWidget::InputText("システムフォント名", editorData_.style.fontFamily);
			}
			styleChanged |= YEditorWidget::DragFloat("文字サイズ", editorData_.style.fontSize, 1.0f, 8.0f, 128.0f, "%.0f");
			styleChanged |= YEditorWidget::Color("文字色", editorData_.style.textColor);
			styleChanged |= YEditorWidget::DragFloat("縁取り", editorData_.style.outlineWidth, 0.5f, 0.0f, 16.0f, "%.1f");
			styleChanged |= YEditorWidget::Color("縁色", editorData_.style.outlineColor);
			styleChanged |= YEditorWidget::DragFloat("文字余白", editorData_.style.textPadding, 1.0f, 0.0f, 128.0f, "%.0f");
			static constexpr const char* alignLabels[] = { "左揃え", "中央揃え", "右揃え" };
			styleChanged |= ImGui::Combo("文字揃え", &editorData_.style.textAlign, alignLabels, IM_ARRAYSIZE(alignLabels));
			styleChanged |= YEditorWidget::Checkbox("文字に影を付ける", editorData_.style.textShadow);
			if (editorData_.style.textShadow) {
				styleChanged |= YEditorWidget::DragVec2("影オフセット", editorData_.style.shadowOffset, 0.5f, -64.0f, 64.0f);
				styleChanged |= YEditorWidget::Color("影色", editorData_.style.shadowColor);
			}

			YEditorWidget::SectionHeader("ページ切り替え");
			styleChanged |= YEditorWidget::DragFloat(
				"表示するときのフェード時間(秒)", editorData_.style.fadeInSeconds,
				0.05f, 0.0f, 5.0f, "%.2f");
			YEditorWidget::HelpMarker("0秒にすると、ページがすぐに表示されます");
			styleChanged |= YEditorWidget::DragFloat(
				"消えるときのフェード時間(秒)", editorData_.style.fadeOutSeconds,
				0.05f, 0.0f, 5.0f, "%.2f");
			YEditorWidget::HelpMarker("この時間をかけて現在のページを消してから、次のページを表示します");

			YEditorWidget::SectionHeader("次へ・スキップUI");
			styleChanged |= YEditorWidget::StringCombo(
				"次へ進むキーボードのキー", editorData_.style.confirmKeyboardKey, keyboardKeys);
			styleChanged |= YEditorWidget::StringCombo(
				"次へ進むゲームパッドのボタン", editorData_.style.confirmGamepadButton, gamepadButtons);
			styleChanged |= YEditorWidget::StringCombo(
				"全体を閉じるキーボードのキー", editorData_.style.skipKeyboardKey, keyboardKeys);
			styleChanged |= YEditorWidget::StringCombo(
				"全体を閉じるゲームパッドのボタン", editorData_.style.skipGamepadButton, gamepadButtons);
			if (editorData_.style.confirmKeyboardKey == editorData_.style.skipKeyboardKey ||
				editorData_.style.confirmGamepadButton == editorData_.style.skipGamepadButton) {
				ImGui::TextColored(
					ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
					"「次へ」と「全体を閉じる」には別のキー・ボタンを設定してください");
			}
			styleChanged |= YEditorWidget::Checkbox("操作案内を表示", editorData_.style.showControlHint);
			if (editorData_.style.showControlHint) {
				styleChanged |= YEditorWidget::Checkbox(
					"選んだキーから操作案内を自動作成", editorData_.style.autoBuildControlHint);
				if (editorData_.style.autoBuildControlHint) {
					ImGui::TextDisabled("表示例: [%s / %s] 次へ",
						editorData_.style.confirmKeyboardKey.c_str(),
						editorData_.style.confirmGamepadButton.c_str());
				}
				else {
					styleChanged |= YEditorWidget::InputText("次への表示文", editorData_.style.hintText);
					styleChanged |= YEditorWidget::InputText("閉じる操作の表示文", editorData_.style.skipHintText);
				}
				styleChanged |= YEditorWidget::StringCombo("次へUI背景画像", editorData_.style.hintPanelTexturePath,
					availablePanelTextures, true);
				styleChanged |= YEditorWidget::Color("次へUI背景色", editorData_.style.hintPanelColor);
				styleChanged |= YEditorWidget::DragFloat("次へ文字サイズ", editorData_.style.hintFontSize,
					1.0f, 8.0f, 128.0f, "%.0f");
				styleChanged |= YEditorWidget::Color("次へ文字色", editorData_.style.hintTextColor);
				styleChanged |= YEditorWidget::DragFloat("次へ文字の縁取り", editorData_.style.hintOutlineWidth,
					0.5f, 0.0f, 16.0f, "%.1f");
				styleChanged |= YEditorWidget::Color("次へ文字の縁色", editorData_.style.hintOutlineColor);
				styleChanged |= YEditorWidget::DragFloat("次へ文字余白", editorData_.style.hintPadding,
					1.0f, 0.0f, 128.0f, "%.0f");
			}
			styleChanged |= YEditorWidget::DragInt("描画レイヤー", editorData_.style.layer, 1, 0, 10000);
		}
		if (styleChanged && editorLivePreview_ && IsPlaying()) {
			currentData_.style = editorData_.style;
			editorPreviewPending_ = true;
			editorPreviewChangeTime_ = ImGui::GetTime();
		}

		if (YEditorWidget::Section section{ "ステップ一覧" }) {
			if (ImGui::Button("追加")) {
				editorData_.steps.push_back(TutorialStep{});
				editorSelectedStep_ = static_cast<int>(editorData_.steps.size()) - 1;
			}
			ImGui::SameLine();
			const bool hasSelection = !editorData_.steps.empty() && editorSelectedStep_ >= 0 &&
				editorSelectedStep_ < static_cast<int>(editorData_.steps.size());
			if (!hasSelection) ImGui::BeginDisabled();
			if (ImGui::Button("複製") && hasSelection) {
				TutorialStep copy = editorData_.steps[editorSelectedStep_];
				copy.name += " コピー";
				editorData_.steps.insert(editorData_.steps.begin() + editorSelectedStep_ + 1, std::move(copy));
				++editorSelectedStep_;
			}
			ImGui::SameLine();
			if (ImGui::Button("上へ") && hasSelection && editorSelectedStep_ > 0) {
				std::swap(editorData_.steps[editorSelectedStep_], editorData_.steps[editorSelectedStep_ - 1]);
				--editorSelectedStep_;
			}
			ImGui::SameLine();
			if (ImGui::Button("下へ") && hasSelection && editorSelectedStep_ + 1 < static_cast<int>(editorData_.steps.size())) {
				std::swap(editorData_.steps[editorSelectedStep_], editorData_.steps[editorSelectedStep_ + 1]);
				++editorSelectedStep_;
			}
			ImGui::SameLine();
			if (ImGui::Button("削除") && hasSelection) {
				editorData_.steps.erase(editorData_.steps.begin() + editorSelectedStep_);
				editorSelectedStep_ = std::min(editorSelectedStep_, static_cast<int>(editorData_.steps.size()) - 1);
			}
			if (!hasSelection) ImGui::EndDisabled();

			ImGui::BeginChild("##tutorialStepList", ImVec2(0.0f, 150.0f), true);
			for (int i = 0; i < static_cast<int>(editorData_.steps.size()); ++i) {
				const std::string label = std::to_string(i + 1) + ". " + editorData_.steps[i].name;
				if (ImGui::Selectable(label.c_str(), editorSelectedStep_ == i)) editorSelectedStep_ = i;
			}
			ImGui::EndChild();
		}

		if (editorData_.steps.empty()) {
			ImGui::Spacing();
			ImGui::TextColored(
				ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
				"このファイルには編集できるページがありません。");
			ImGui::TextWrapped(
				"ページを追加すると、ページごとのレイアウト・説明文・画像UIを設定できます。");
			if (ImGui::Button("最初のページを追加して編集する")) {
				editorData_.steps.push_back(TutorialStep{});
				editorSelectedStep_ = 0;
				editorStatus_ = "最初のページを追加しました。保存ボタンでJSONへ保存できます";
			}
		}

		if (!editorData_.steps.empty()) {
			editorSelectedStep_ = std::clamp(editorSelectedStep_, 0, static_cast<int>(editorData_.steps.size()) - 1);
			TutorialStep& step = editorData_.steps[editorSelectedStep_];
			if (YEditorWidget::Section section{ "選択ステップの編集" }) {
				bool stepChanged = false;

				YEditorWidget::SectionHeader("このページだけのレイアウト");
				ImGui::TextDisabled("ここで変更した位置・サイズは、選択中のページにだけ保存されます");
				if (ImGui::Button("画面下に配置")) {
					step.layout = TutorialStepLayout{};
					stepChanged = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("画面上に配置")) {
					step.layout.panelPosition = { 640.0f, 150.0f };
					step.layout.panelSize = { 1120.0f, 240.0f };
					step.layout.textOffset = { 0.0f, -35.0f };
					step.layout.textMaxWidth = 1020.0f;
					step.layout.hintOffset = { 0.0f, 85.0f };
					step.layout.hintPanelSize = { 540.0f, 54.0f };
					stepChanged = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("中央に配置")) {
					step.layout.panelPosition = { 640.0f, 360.0f };
					step.layout.panelSize = { 840.0f, 320.0f };
					step.layout.textOffset = { 0.0f, -45.0f };
					step.layout.textMaxWidth = 760.0f;
					step.layout.hintOffset = { 0.0f, 115.0f };
					step.layout.hintPanelSize = { 540.0f, 54.0f };
					stepChanged = true;
				}
				if (editorSelectedStep_ > 0 && ImGui::Button("前のページからレイアウトをコピー")) {
					step.layout = editorData_.steps[editorSelectedStep_ - 1].layout;
					stepChanged = true;
				}
				stepChanged |= YEditorWidget::DragVec2(
					"説明パネルの中心位置", step.layout.panelPosition, 1.0f, -2048.0f, 4096.0f);
				stepChanged |= YEditorWidget::DragVec2(
					"説明パネルのサイズ", step.layout.panelSize, 1.0f, 1.0f, 4096.0f);
				stepChanged |= YEditorWidget::DragVec2(
					"説明文の相対位置", step.layout.textOffset, 1.0f, -2048.0f, 2048.0f);
				YEditorWidget::HelpMarker("説明パネルの中心を基準にした位置です");
				stepChanged |= YEditorWidget::DragFloat(
					"説明文の最大幅", step.layout.textMaxWidth, 1.0f, 64.0f, 4096.0f, "%.0f");
				stepChanged |= YEditorWidget::DragVec2(
					"操作案内の相対位置", step.layout.hintOffset, 1.0f, -2048.0f, 2048.0f);
				YEditorWidget::HelpMarker("説明パネルの中心を基準にした位置です");
				stepChanged |= YEditorWidget::DragVec2(
					"操作案内の背景サイズ", step.layout.hintPanelSize, 1.0f, 1.0f, 4096.0f);

				YEditorWidget::SectionHeader("このページの内容");
				stepChanged |= YEditorWidget::InputText("管理名", step.name);
				stepChanged |= YEditorWidget::InputText("見出し", step.speaker);
				stepChanged |= YEditorWidget::InputTextMultiline("第三者へ表示する説明文", step.text, 6);
				YEditorWidget::HelpMarker(
					"初めて遊ぶ学生が、このページだけを読んでも操作と目的を理解できる文章にしてください");

				YEditorWidget::SectionHeader("完了条件");
				ImGui::TextDisabled("プレイヤーが実際に行動したら次へ進めたい場合はこちらを使います");
				TutorialSignal::GetInstance()->ConnectEngineSources();
				stepChanged |= DrawConditionEditor(
					step.complete, TutorialSignal::GetInstance()->GetKnownNames(), 0);

				const bool usesCondition = step.complete.type != TutorialConditionType::None;
				if (usesCondition) {
					ImGui::TextDisabled("完了条件を設定したため、下の旧設定は使用されません");
				}
				ImGui::BeginDisabled(usesCondition);

				static constexpr const char* waitLabels[] = { "決定入力を待つ", "指定秒数を待つ", "ゲームイベントを待つ" };
				int waitType = static_cast<int>(step.waitType);
				if (ImGui::Combo("完了条件", &waitType, waitLabels, IM_ARRAYSIZE(waitLabels))) {
					step.waitType = static_cast<TutorialWaitType>(waitType);
					stepChanged = true;
				}
				if (step.waitType == TutorialWaitType::Seconds) {
					stepChanged |= YEditorWidget::DragFloat("待ち時間(秒)", step.waitSeconds, 0.1f, 0.0f, 60.0f, "%.1f");
				}
				else if (step.waitType == TutorialWaitType::Event) {
					const auto& knownEvents = GetKnownEventNames();
					if (!knownEvents.empty()) {
						stepChanged |= YEditorWidget::StringCombo("イベント名", step.eventName, knownEvents);
					}
					const bool eventIsKnown = std::find(knownEvents.begin(), knownEvents.end(), step.eventName) != knownEvents.end();
					if (knownEvents.empty() || !eventIsKnown) {
						stepChanged |= YEditorWidget::InputText("新しいイベント名", step.eventName);
						if (!step.eventName.empty() && ImGui::SmallButton("イベント候補に登録")) {
							RegisterEventName(step.eventName);
						}
					}
					else if (ImGui::SmallButton("一覧にないイベントを追加")) {
						step.eventName.clear();
					}
					YEditorWidget::HelpMarker("ゲーム側から TutorialManager::NotifyEvent(イベント名) を呼ぶと次へ進みます");
				}
				ImGui::EndDisabled();
				if (!uiIds.empty()) stepChanged |= YEditorWidget::StringCombo("強調するUI", step.targetUIId, uiIds, true);
				else stepChanged |= YEditorWidget::InputText("強調するUI ID", step.targetUIId);
				YEditorWidget::HelpMarker("UI管理に登録されているIDを指定すると、そのUIをパルス表示します");

				YEditorWidget::SectionHeader("スポットライト");
				stepChanged |= DrawSpotlightEditor(step.spotlight, uiIds);

				stepChanged |= YEditorWidget::Checkbox("ゲームを一時停止", step.pauseGameplay);
				stepChanged |= YEditorWidget::Checkbox("チュートリアル全体を閉じられる", step.skippable);

				YEditorWidget::SectionHeader("このページと一緒に表示する画像UI");
				ImGui::TextDisabled("操作図やキー画像などを、本文とは別の位置へ複数配置できます");
				if (ImGui::Button("画像UIを追加")) {
					step.additionalUIs.push_back(TutorialStepUI{});
					stepChanged = true;
				}
				for (int i = 0; i < static_cast<int>(step.additionalUIs.size()); ++i) {
					TutorialStepUI& additional = step.additionalUIs[i];
					ImGui::PushID(i);
					const std::string header = std::to_string(i + 1) + ". " + additional.name;
					if (ImGui::TreeNode(header.c_str())) {
						stepChanged |= YEditorWidget::InputText("管理名", additional.name);
						stepChanged |= YEditorWidget::StringCombo(
							"表示する画像", additional.texturePath, availablePanelTextures, true);
						stepChanged |= YEditorWidget::DragVec2(
							"画面上の中心位置", additional.position, 1.0f, -2048.0f, 4096.0f);
						stepChanged |= YEditorWidget::DragVec2(
							"表示サイズ", additional.size, 1.0f, 1.0f, 4096.0f);
						stepChanged |= YEditorWidget::DragVec2(
							"画像の基準点", additional.anchorPoint, 0.01f, 0.0f, 1.0f);
						YEditorWidget::HelpMarker("中央に置く場合は X=0.5、Y=0.5 にします");
						stepChanged |= YEditorWidget::Color("画像の色・透明度", additional.color);
						stepChanged |= YEditorWidget::DragInt(
							"本文パネルからの描画順", additional.layerOffset, 1, -100, 100);
						if (ImGui::Button("この画像UIを削除")) {
							step.additionalUIs.erase(step.additionalUIs.begin() + i);
							--i;
							stepChanged = true;
							ImGui::TreePop();
							ImGui::PopID();
							continue;
						}
						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				if (stepChanged && editorLivePreview_ && IsPlaying() &&
					currentStep_ == static_cast<std::size_t>(editorSelectedStep_)) {
					currentData_.steps[currentStep_] = step;
					runtimeUIDirty_ = true;
				}
			}
		}
	}

} // namespace YoRigine

#endif // USE_IMGUI
