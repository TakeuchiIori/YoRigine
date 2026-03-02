#pragma once
#include "YEmitterGroup.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

/// <summary>
/// YEmitterGroup を一元管理するシングルトン
/// Update / 保存・読み込み / グループ作成・削除を担う
/// </summary>
class YEmitterGroupManager {
public:
	//=================================================================
	// シングルトン
	//=================================================================

	static YEmitterGroupManager& GetInstance() {
		static YEmitterGroupManager instance;
		return instance;
	}

	YEmitterGroupManager(const YEmitterGroupManager&) = delete;
	YEmitterGroupManager& operator=(const YEmitterGroupManager&) = delete;

	//=================================================================
	// グループ管理
	//=================================================================

	/// <summary>
	/// グループを作成（同名が既存なら既存を返す）
	/// </summary>
	YEmitterGroup* CreateGroup(const std::string& name);

	/// <summary>
	/// グループを取得（なければ nullptr）
	/// </summary>
	YEmitterGroup* GetGroup(const std::string& name);

	/// <summary>
	/// グループを削除
	/// </summary>
	void RemoveGroup(const std::string& name) {
		groups_.erase(name);
	}

	/// <summary>
	/// 全グループ名リスト
	/// </summary>
	std::vector<std::string> GetAllGroupNames() const;

	// グループ数の取得
	size_t GetGroupCount() const { return groups_.size(); }

	//=================================================================
	// 更新
	//=================================================================

	void Update(float deltaTime);

	//=================================================================
	// JSON 保存・読み込み
	//=================================================================


	// セーブするものをJsonに書き出す
	nlohmann::json SaveAllToJson() const;

	// 実際にセーブする処理
	bool SaveAllToFile(const std::string& filePath) const;

	// Jsonから読み込む
	void LoadAllFromJson(const nlohmann::json& j);

	// 実際の読み込み処理
	bool LoadAllFromFile(const std::string& filePath);
private:
	YEmitterGroupManager() = default;
	~YEmitterGroupManager() = default;

	std::unordered_map<std::string, std::unique_ptr<YEmitterGroup>> groups_;
};