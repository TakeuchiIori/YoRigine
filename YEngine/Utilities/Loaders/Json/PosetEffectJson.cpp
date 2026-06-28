#include "PosetEffectJson.h"
#include <filesystem>
#include "Debugger/Logger.h"

PosetEffectJson::PosetEffectJson(const std::string& fileName, const std::string& folderPath)
	: fileName_(fileName), folderPath_(folderPath)
{

	displayName_ = fileName_;
	// ファイルが見つからなかったら作成
	if (!std::filesystem::exists(folderPath))
	{
		std::filesystem::create_directories(folderPath);
	}

	std::string fullKey = MakeFullPath(folderPath_, fileName_);

	if (instances.find(fullKey) == instances.end())
	{
		instances[fullKey] = this;
		LoadAll();
	}

}

PosetEffectJson::~PosetEffectJson()
{
	if (instances[fileName_] == this) // 自分が管理対象なら削除
	{
		instances.erase(fileName_);
	}
}
void PosetEffectJson::Unregister(const std::string& name)
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

void PosetEffectJson::Reset(bool clearVariables)
{
	if (clearVariables)
	{
		variables_.clear();  // すべての変数を削除
	} else
	{
		for (auto& pair : variables_)
		{
			pair.second->ResetValue();  // 初期値にリセット
		}
	}

	// JSON ファイルを空にする
	std::string fullPath = MakeFullPath(folderPath_, fileName_);
	std::ofstream ofs(fullPath, std::ofstream::trunc);
	ofs.close();
}

void PosetEffectJson::Save()
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

void PosetEffectJson::LoadAll()
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


void PosetEffectJson::ImGuiManager()
{
#ifdef USE_IMGUI

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("オプション")) {

			if (ImGui::MenuItem("全て保存")) {
				for (auto& [name, instance] : instances) {
					instance->Save();
				}
			}

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static char filterBuffer[128] = "";
	ImGui::InputTextWithHint("##Filter", "クラス名でフィルター", filterBuffer, IM_ARRAYSIZE(filterBuffer));

	std::map<std::string, std::map<std::string, std::vector<std::string>>> treeMap;
	for (const auto& [name, manager] : instances) {
		std::string cat = manager->GetCategory().empty() ? "Uncategorized" : manager->GetCategory();
		std::string subCat = manager->GetSubCategory().empty() ? "__NoSubCategory__" : manager->GetSubCategory();
		if (strlen(filterBuffer) == 0 || name.find(filterBuffer) != std::string::npos) {
			treeMap[cat][subCat].push_back(name);
		}
	}

	ImGui::BeginChild("ClassTree", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	for (const auto& [cat, subMap] : treeMap) {
		if (ImGui::CollapsingHeader(cat.c_str(), ImGuiTreeNodeFlags_None)) {
			for (const auto& [subCat, classList] : subMap) {
				if (subCat != "__NoSubCategory__") {
					if (ImGui::TreeNode(subCat.c_str())) {
						for (const auto& className : classList) {
							if (ImGui::Selectable(className.c_str(), selectedClass == className)) {
								selectedClass = className;
							}
						}
						ImGui::TreePop();
					}
				} else {
					for (const auto& className : classList) {
						if (ImGui::Selectable(className.c_str(), selectedClass == className)) {
							selectedClass = className;
						}
					}
				}
			}
		}
	}
	ImGui::EndChild();

	if (!selectedClass.empty()) {
		auto it = instances.find(selectedClass);
		if (it != instances.end()) {
			PosetEffectJson* instance = it->second;

			ImGui::SeparatorText(selectedClass.c_str());
			ImGui::PushID(selectedClass.c_str());
			ImGui::BeginChild("VariableList", ImVec2(0, 300), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

			std::map<std::string, std::vector<std::pair<std::string, IVariableJson*>>> groupedVars;
			std::vector<std::pair<std::string, IVariableJson*>> flatVars;
			for (auto& [key, var] : instance->variables_) {
				if (instance->treeKeys_.contains(key)) {
					size_t dotPos = key.find('.');
					std::string group = key.substr(0, dotPos);
					std::string subKey = key.substr(dotPos + 1);
					groupedVars[group].emplace_back(subKey, var.get());
				} else {
					flatVars.emplace_back(key, var.get());
				}
			}
			for (auto& [group, vars] : groupedVars) {
				if (ImGui::TreeNode(group.c_str())) {
					for (auto& [name, var] : vars) {
						var->ShowImGui(name, selectedClass);
					}
					ImGui::TreePop();
				}
			}
			for (auto& [key, var] : flatVars) {
				var->ShowImGui(key, selectedClass);
			}
			ImGui::EndChild();

			if (ImGui::Button("保存")) {
				std::string message = format("{}.json Saved!!.", selectedClass);
				MessageBoxA(nullptr, message.c_str(), "PosetEffectJson", 0);
				instance->Save();
			}
			ImGui::PopID();
		}
	}
#endif
}

void PosetEffectJson::ClearRegister(std::string parentFileName)
{
	if (instances.find(parentFileName) != instances.end()) // 親ファイルがあるかチェック
	{
		instances[parentFileName]->variables_.clear();
	}
}


std::string PosetEffectJson::MakeFullPath(const std::string& folder, const std::string& file) const
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
