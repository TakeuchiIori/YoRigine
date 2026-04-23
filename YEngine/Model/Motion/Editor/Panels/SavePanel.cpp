#include "SavePanel.h"
#include "Model.h"
#include "Object3D/ObjectManager.h"
#include "../../Core/Motion.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace fs = std::filesystem;

void SavePanel::Initialize(MotionEditorContext* context)
{
	context_ = context;
	binaryBrowser_.currentDirectory = "Resources/Binary";
	binaryBrowser_.filterExtension = ".anim";
}

void SavePanel::DrawImGui()
{
#ifdef USE_IMGUI
	if (binaryBrowser_.isOpen) {
		DrawFileBrowser(binaryBrowser_, "バイナリファイルを選択");
		if (!binaryBrowser_.isOpen && !binaryBrowser_.selectedFilePath.empty())
			savePath_ = binaryBrowser_.selectedFilePath;
	}

	DrawSaveLoadPopup();
#endif
}

void SavePanel::DrawSaveLoadPopup()
{
#ifdef USE_IMGUI
	if (context_->showSavePopup) ImGui::OpenPopup("保存 / 読み込み##popup");

	ImGui::SetNextWindowSize(ImVec2(500, 280), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("保存 / 読み込み##popup", &context_->showSavePopup)) {

		ImGui::Text("バイナリファイル (.anim):");
		{
			char buf[512]; strncpy_s(buf, sizeof(buf), savePath_.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
			if (ImGui::InputText("##savepath", buf, sizeof(buf))) savePath_ = buf;
			ImGui::SameLine();
			if (ImGui::Button("参照##b")) {
				binaryBrowser_.isOpen = true;
				binaryBrowser_.selectedFilePath = "";
				context_->showSavePopup = false;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

		bool canSave = context_->currentMotion != nullptr;
		if (!canSave) ImGui::BeginDisabled();
		if (ImGui::Button("バイナリ保存", ImVec2(130, 0))) {
			try {
				if (context_->currentMotion) {
					context_->currentMotion->SaveBinary(*context_->currentMotion, AnimDisplayName(context_->selectedAnimKey), savePath_);
					saveMsg_ = "保存成功: " + savePath_;
					context_->statusMsg = saveMsg_;
				}
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("保存失敗: ") + e.what();
			}
		}
		if (!canSave) ImGui::EndDisabled();

		ImGui::SameLine(0, 10);
		Object3d* target = context_->GetTargetObject();

		if (ImGui::Button("バイナリ読み込み", ImVec2(140, 0))) {
			try {
				Motion loaded = Motion().LoadBinary(savePath_);
				const std::string key = "Binary:" + savePath_;
				Model::animationCache_[key] = std::move(loaded);
				context_->selectedAnimKey = key;
				context_->currentMotion = &Model::animationCache_[key];
				if (target) {
					target->SetChangeMotion(context_->loadFileName, MotionPlayMode::Loop, AnimDisplayName(key));
				}
				saveMsg_ = "読み込み成功: " + savePath_;
				context_->statusMsg = saveMsg_;
				context_->requireTimelineRebuild = true;
			}
			catch (const std::exception& e) {
				saveMsg_ = std::string("読み込み失敗: ") + e.what();
			}
		}

		if (!saveMsg_.empty()) {
			ImGui::Spacing();
			bool isError = saveMsg_.find("失敗") != std::string::npos;
			ImGui::TextColored(isError ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(0.3f, 1, 0.3f, 1), "%s", saveMsg_.c_str());
		}

		ImGui::Spacing(); ImGui::Separator();
		if (ImGui::Button("閉じる", ImVec2(100, 0))) {
			context_->showSavePopup = false;
			saveMsg_ = "";
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	if (!context_->showSavePopup) ImGui::CloseCurrentPopup();
#endif
}

#ifdef USE_IMGUI
void SavePanel::DrawFileBrowser(FileBrowserState& state, const char* title)
{
	ImGui::SetNextWindowSize(ImVec2(540, 420), ImGuiCond_FirstUseEver);
	bool open = true;
	if (!ImGui::Begin(title, &open)) { ImGui::End(); if (!open) state.isOpen = false; return; }

	ImGui::TextColored(ImVec4(0.7f, 0.9f, 1, 1), "%s", state.currentDirectory.c_str());
	ImGui::Separator();

	bool canUp = !state.directoryHistory.empty();
	if (!canUp) ImGui::BeginDisabled();
	if (ImGui::Button("[ .. ] 上へ")) {
		state.currentDirectory = state.directoryHistory.back();
		state.directoryHistory.pop_back();
		state.selectedFilePath = "";
	}
	if (!canUp) ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("| ダブルクリックでフォルダを開く / ファイルを選択");
	ImGui::Separator();

	auto entries = GetDirectoryEntries(state.currentDirectory, state.filterExtension);
	ImGui::BeginChild("##fblist", ImVec2(0, -50), true);
	for (const auto& e : entries) {
		std::string name = e.path().filename().string();
		if (e.is_directory()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1, 1));
			std::string lbl = "[DIR] " + name;
			if (ImGui::Selectable(lbl.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
				if (ImGui::IsMouseDoubleClicked(0)) {
					state.directoryHistory.push_back(state.currentDirectory);
					state.currentDirectory = e.path().string();
					std::replace(state.currentDirectory.begin(), state.currentDirectory.end(), '\\', '/');
					state.selectedFilePath = "";
				}
			}
			ImGui::PopStyleColor();
		}
		else {
			bool isSel = (state.selectedFilePath == e.path().string());
			if (ImGui::Selectable(name.c_str(), isSel, ImGuiSelectableFlags_AllowDoubleClick)) {
				state.selectedFilePath = e.path().string();
				std::replace(state.selectedFilePath.begin(), state.selectedFilePath.end(), '\\', '/');
				if (ImGui::IsMouseDoubleClicked(0)) { state.isOpen = false; ImGui::EndChild(); ImGui::End(); return; }
			}
		}
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextUnformatted(state.selectedFilePath.empty() ? "(ファイルが未選択です)" : state.selectedFilePath.c_str());
	float btnX = ImGui::GetContentRegionAvail().x - 120;
	ImGui::SameLine(btnX);
	bool canOK = !state.selectedFilePath.empty();
	if (!canOK) ImGui::BeginDisabled();
	if (ImGui::Button("OK", ImVec2(55, 0))) state.isOpen = false;
	if (!canOK) ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("キャンセル", ImVec2(60, 0))) { state.selectedFilePath = ""; state.isOpen = false; }

	if (!open) state.isOpen = false;
	ImGui::End();
}
#endif

std::string SavePanel::AnimDisplayName(const std::string& key)
{
	auto pos = key.find('#');
	return (pos != std::string::npos) ? key.substr(pos + 1) : key;
}

std::vector<std::filesystem::directory_entry> SavePanel::GetDirectoryEntries(const std::string& dir, const std::string& ext) const
{
	std::vector<fs::directory_entry> dirs, files;
	std::error_code ec;
	for (const auto& e : fs::directory_iterator(dir, ec)) {
		if (ec) break;
		if (e.is_directory()) { dirs.push_back(e); }
		else if (e.is_regular_file()) {
			std::string ex = e.path().extension().string();
			if (ext.empty()) {
				if (ex == ".gltf" || ex == ".obj" || ex == ".anim") files.push_back(e);
			}
			else {
				if (ex == ext) files.push_back(e);
			}
		}
	}
	auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
		return a.path().filename().string() < b.path().filename().string(); };
	std::sort(dirs.begin(), dirs.end(), byName);
	std::sort(files.begin(), files.end(), byName);
	dirs.insert(dirs.end(), files.begin(), files.end());
	return dirs;
}