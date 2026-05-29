#include "YEmitterGroupManager.h"

#include <fstream>

/// <summary>
/// エミッタグループの生成
/// </summary>
/// <param name="name"></param>
/// <returns></returns>
YEmitterGroup* YEmitterGroupManager::CreateGroup(const std::string& name) {
	if (groups_.count(name)) return groups_[name].get();
	auto g = std::make_unique<YEmitterGroup>(name);
	YEmitterGroup* ptr = g.get();
	groups_[name] = std::move(g);
	return ptr;
}

/// <summary>
/// 指定のエミッタグループ取得
/// </summary>
/// <param name="name"></param>
/// <returns></returns>
YEmitterGroup* YEmitterGroupManager::GetGroup(const std::string& name) {
	auto it = groups_.find(name);
	return (it != groups_.end()) ? it->second.get() : nullptr;
}

/// <summary>
/// 全てのエミッタグループの名前取得
/// </summary>
/// <returns></returns>
std::vector<std::string> YEmitterGroupManager::GetAllGroupNames() const {
	std::vector<std::string> names;
	names.reserve(groups_.size());
	for (const auto& [name, _] : groups_) names.push_back(name);
	return names;
}

/// <summary>
/// 更新処理
/// </summary>
/// <param name="deltaTime"></param>
void YEmitterGroupManager::Update(float deltaTime) {
	for (auto& [name, group] : groups_) {
		group->Update(deltaTime);
	}
}

//=================================================================
// JSON 保存・読み込み
//=================================================================


// セーブするものをJsonに書き出す
nlohmann::json YEmitterGroupManager::SaveAllToJson() const {
	nlohmann::json j;
	j["groups"] = nlohmann::json::array();
	for (const auto& [name, group] : groups_) {
		j["groups"].push_back(group->SaveToJson());
	}
	return j;
}

// 実際にセーブを行う
bool YEmitterGroupManager::SaveAllToFile(const std::string& filePath) const {
	try {
		std::ofstream ofs(filePath);
		if (!ofs.is_open()) return false;
		ofs << SaveAllToJson().dump(4);
		return true;
	}
	catch (...) { return false; }
}

// Jsonから読み込む
void YEmitterGroupManager::LoadAllFromJson(const nlohmann::json& j) {
	if (!j.contains("groups")) return;
	for (const auto& gj : j["groups"]) {
		std::string name = gj.value("groupName", "unnamed");
		auto* group = CreateGroup(name);
		group->LoadFromJson(gj);
	}
}

// 実際に読み込む
bool YEmitterGroupManager::LoadAllFromFile(const std::string& filePath) {
	try {
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) return false;
		nlohmann::json j;
		ifs >> j;
		LoadAllFromJson(j);
		return true;
	}
	catch (...) { return false; }
}

// 指定グループを単独ファイルに保存
bool YEmitterGroupManager::SaveGroupToFile(const std::string& groupName, const std::string& filePath) const {
	auto it = groups_.find(groupName);
	if (it == groups_.end()) return false;
	try {
		std::ofstream ofs(filePath);
		if (!ofs.is_open()) return false;
		ofs << it->second->SaveToJson().dump(4);
		return true;
	}
	catch (...) { return false; }
}

// JSON から単独グループを読み込み
void YEmitterGroupManager::LoadGroupFromJson(const nlohmann::json& j) {
	std::string name = j.value("groupName", "unnamed");
	auto* group = CreateGroup(name);
	group->LoadFromJson(j);
}

// ファイルから単独グループを読み込み（形式を自動判別）
bool YEmitterGroupManager::LoadGroupFromFile(const std::string& filePath) {
	try {
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) return false;
		nlohmann::json j;
		ifs >> j;
		if (j.contains("groupName")) {
			LoadGroupFromJson(j);
		}
		else if (j.contains("groups")) {
			LoadAllFromJson(j);
		}
		return true;
	}
	catch (...) { return false; }
}
