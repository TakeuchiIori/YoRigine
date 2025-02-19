#include "JsonManager.h"

JsonManager::JsonManager(const std::string& fileName, const std::string& folderPath)
	: fileName_(fileName), folderPath_(folderPath)
{
	// マネージャ生成時に自動でロードを実行
	//LoadAll();
	if (instances.find(fileName) == instances.end()) // 重複チェック
	{
		instances[fileName] = this; // 一意のインスタンスのみ登録
		LoadAll();
	}
}

JsonManager::~JsonManager()
{
	if (instances[fileName_] == this) // 自分が管理対象なら削除
	{
		instances.erase(fileName_);
	}
}

void JsonManager::Reset(bool clearVariables)
{
	if (clearVariables)
	{
		variables_.clear();  // すべての変数を削除
	} else
	{
		for (auto& pair : variables_)
		{
			pair.second->ResetValue();  // 初期値にリセット（新しい関数を追加）
		}
	}

	// JSON ファイルを空にする
	std::string fullPath = MakeFullPath(folderPath_, fileName_);
	std::ofstream ofs(fullPath, std::ofstream::trunc);
	ofs.close();
}

void JsonManager::Save()
{
	nlohmann::json jsonData;

	// 全変数を JSON に書き込む
	for (auto& pair : variables_)
	{
		const std::string& name = pair.first;
		auto& variablePtr = pair.second;
		variablePtr->SaveToJson(jsonData[name]);
	}

	// フルパス生成（フォルダパス + "/" + ファイル名）
	std::string fullPath = MakeFullPath(folderPath_, fileName_);

	// ファイルに書き出し
	std::ofstream ofs(fullPath);
	if (!ofs)
	{
		std::cerr << "ファイルを開けませんでした: " << fullPath << std::endl;
		return;
	}
	ofs << jsonData.dump(4); // インデント4で整形して出力
	ofs.close();
}

void JsonManager::LoadAll()
{
	std::string fullPath = MakeFullPath(folderPath_, fileName_);
	std::ifstream ifs(fullPath);
	if (!ifs)
	{
		// ファイルが存在しない場合などは何もしない（新規作成扱い）
		return;
	}

	// ファイルサイズをチェックするために末尾にシーク
	ifs.seekg(0, std::ios::end);
	std::streampos fileSize = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	// ファイルサイズが 0（空）なら、新規ファイルとして扱う
	if (fileSize == 0)
	{
		// いったん閉じる
		ifs.close();

		// 登録された変数で Save()（=「初期値をJSONとして書き出し」）し、終了
		Save();
		return;
	}

	// JSON として読み込み
	nlohmann::json jsonData;
	ifs >> jsonData;
	ifs.close();

	// JSON から各変数に反映
	for (auto& pair : variables_)
	{
		const std::string& name = pair.first;
		auto& variablePtr = pair.second;
		if (jsonData.contains(name))
		{
			variablePtr->LoadFromJson(jsonData[name]);
		}
	}
}

void JsonManager::ImGui(const std::string& className)
{
#ifdef _DEBUG


	auto it = instances.find(className);
	if (it == instances.end()) return; // クラスが存在しない場合は何もしない

	JsonManager* instance = it->second;

	std::string windowTitle = "JsonManager - " + className;
	ImGui::Begin(windowTitle.c_str());

	// クラス名を強調
	ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "[ %s ]", className.c_str());
	ImGui::Separator();

	ImGui::PushID(className.c_str()); // クラスごとにIDを設定

	for (auto& pair : instance->variables_)
	{
		pair.second->ShowImGui(pair.first, className);
	}

	ImGui::Separator();

	// ボタンをセンタリング
	float buttonWidth = 120.0f;
	float windowWidth = ImGui::GetWindowWidth();
	float buttonPosX = (windowWidth - buttonWidth * 2) * 0.5f;

	ImGui::SetCursorPosX(buttonPosX);
	if (ImGui::Button(("Reset " + className).c_str(), ImVec2(buttonWidth, 0)))
	{
		instance->Reset();
	}
	ImGui::SameLine();
	if (ImGui::Button(("Clear All " + className).c_str(), ImVec2(buttonWidth, 0)))
	{
		instance->Reset(true);
	}

	ImGui::PopID();
	ImGui::End();

#endif // _DEBUG
}

void JsonManager::ImGuiManager()
{
#ifdef _DEBUG
	ImGui::Begin("JsonManager"); // Parent window

	ImGui::Text("Select Class:");
	ImGui::Separator();

	// Start child window with scrollbar
	ImGui::BeginChild("ClassList", ImVec2(0, 150), true);

	// Display class name buttons vertically
	for (auto& instance : instances) {
		ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 20.0f);
		if (ImGui::Button(instance.first.c_str(),
			ImVec2(250, 30))) // Unified button width
		{
			selectedClass = instance.first; // Update selected class
		}
		ImGui::PopTextWrapPos();
	}

	ImGui::EndChild();  // End child window
	ImGui::Separator(); // Separator line

	// Display only the selected class
	if (!selectedClass.empty()) {
		auto it = instances.find(selectedClass);
		if (it != instances.end()) {
			JsonManager* instance = it->second;

			std::string windowTitle = "JsonManager - " + selectedClass;
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "[ %s ]",
				selectedClass.c_str());

			ImGui::Separator();

			ImGui::PushID(selectedClass.c_str()); // Set ID for each class

			// Start child window with scrollbar for variables
			ImGui::BeginChild("VariableList", ImVec2(0, 200), true);

			for (auto& pair : instance->variables_) {
				pair.second->ShowImGui(pair.first, selectedClass);
			}

			ImGui::EndChild(); // End child window for variables

			if (ImGui::Button(("Save " + selectedClass).c_str())) {
				std::string message = format("{}.json Saved!!.", selectedClass);
				MessageBoxA(nullptr, message.c_str(), "JsonManager", 0);
				instance->Save();
			}
			ImGui::PopID();
		}
	}

	ImGui::End();
#endif
}

std::string JsonManager::MakeFullPath(const std::string& folder, const std::string& file) const
{
	// fileName_ に拡張子 .json が付いていない場合は付与する
	std::string finalFileName = file;
	static const std::string extension = ".json";
	if (finalFileName.size() < extension.size() ||
		finalFileName.compare(finalFileName.size() - extension.size(), extension.size(), extension) != 0)
	{
		finalFileName += extension;
	}

	// フォルダパスが空なら、ファイル名だけ返す
	if (folder.empty())
	{
		return finalFileName;
	}

	// フォルダパス末尾に '/' or '\\' がなければ追加
	char lastChar = folder[folder.size() - 1];
	if (lastChar == '/' || lastChar == '\\')
	{
		return folder + finalFileName;
	} else
	{
		return folder + "/" + finalFileName;
	}
}
