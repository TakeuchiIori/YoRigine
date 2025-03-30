#include "JsonManager.h"
#include <filesystem>

JsonManager::JsonManager(const std::string& fileName, const std::string& folderPath)
	: fileName_(fileName), folderPath_(folderPath)
{
	
	// フォルダが存在しない場合は作成
	if (!std::filesystem::exists(folderPath))
	{
		std::filesystem::create_directories(folderPath);
	}

	// 重複チェック
	if (instances.find(fileName) == instances.end()) 
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
void JsonManager::Unregister(const std::string& name)
{
	// 変数が存在する場合、削除
	auto it = variables_.find(name);
	if (it != variables_.end())
	{
		variables_.erase(it);
	}

	// JSONファイルから削除
	std::string fullPath = MakeFullPath(folderPath_, fileName_);
	std::ifstream ifs(fullPath);
	if (!ifs)
	{
		std::cerr << "ファイルを開けませんでした: " << fullPath << std::endl;
		return;
	}

	nlohmann::json jsonData;
	ifs >> jsonData;
	ifs.close();

	// JSONデータから該当のキーを削除
	if (jsonData.contains(name))
	{
		jsonData.erase(name);
	}

	// 更新されたJSONデータを保存
	std::ofstream ofs(fullPath);
	if (!ofs)
	{
		std::cerr << "ファイルを開けませんでした: " << fullPath << std::endl;
		return;
	}
	ofs << jsonData.dump(4);
	ofs.close();
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

// JsonManager.cpp の ImGuiManager を修正
void JsonManager::ImGuiManager()
{
#ifdef _DEBUG
	ImGui::Begin("JsonManager"); // 親ウィンドウ

	ImGui::Text("Select Category:");
	//ImGui::Separator();

	// カテゴリごとのマップを作る
	std::unordered_map<std::string, std::vector<std::string>> categoryMap;
	for (const auto& [name, manager] : instances) {
		std::string category = manager->GetCategory().empty() ? "Uncategorized" : manager->GetCategory();
		categoryMap[category].push_back(name);
	}

	// クラス選択スクロール部分（固定せず最大限使う）
	ImGui::BeginChild("ClassList", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	for (const auto& [category, classList] : categoryMap) {
		if (ImGui::TreeNode(category.c_str())) {
			for (const auto& className : classList) {
				if (ImGui::Button(className.c_str(), ImVec2(250, 30))) {
					selectedClass = className;
				}
			}
			ImGui::TreePop();
		}
	}

	ImGui::EndChild(); // クラス一覧パネル終わり

	//ImGui::Separator();

	// 選択されたクラスを表示
	if (!selectedClass.empty()) {
		auto it = instances.find(selectedClass);
		if (it != instances.end()) {
			JsonManager* instance = it->second;

			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "[ %s ]", selectedClass.c_str());
			//ImGui::Separator();

			ImGui::PushID(selectedClass.c_str());

			// 変数表示もスクロールできるように
			ImGui::BeginChild("VariableList", ImVec2(0, 200), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

			for (auto& pair : instance->variables_) {
				pair.second->ShowImGui(pair.first, selectedClass);
			}

			ImGui::EndChild();

			if (ImGui::Button(("Save " + selectedClass).c_str())) {
				std::string message = format("{}.json Saved!!.", selectedClass);
				MessageBoxA(nullptr, message.c_str(), "JsonManager", 0);
				instance->Save();
			}

			ImGui::PopID();
		}
	}

	ImGui::End(); // 親ウィンドウ終わり
#endif
}

void JsonManager::ChildReset(std::string parentFileName, std::string childName)
{
	auto it = instances.find(selectedClass);
	if (it != instances.end()) {
		JsonManager* instance = it->second;


		if (instance->child_.empty())  // childが空なら
		{
			return;
		} else  // childがある場合
		{
			std::unordered_map<std::string, std::vector<std::pair<std::string, IVariableJson*>>> groupedVars;

			// 変数を child_ のキーごとに分類
			for (auto& pair : instance->variables_)
			{
				for (auto& CHPair : instance->child_)
				{
					if (pair.first.length() >= CHPair.first.size() &&
						pair.first.compare(0, CHPair.first.size(), CHPair.first) == 0)
					{
						groupedVars[CHPair.first].emplace_back(pair.first, pair.second.get());
						break; // 1つの `child_` にマッチすれば十分
					}
				}
			}

			// 各 child_ のチェックボックスと対応する変数表示
			for (auto& CHPair : instance->child_)
			{
				ImGui::Checkbox(CHPair.first.c_str(), &CHPair.second);
				if (CHPair.second) // ON の場合
				{
					for (auto& var : groupedVars[CHPair.first])
					{
						instance->variables_.erase(var.first);
					}
				}
			}
		}
	}
}

void JsonManager::ClearRegister(std::string parentFileName)
{
	if (instances.find(parentFileName) != instances.end()) // 親ファイルがあるかチェック
	{
		instances[parentFileName]->variables_.clear();
	}
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
