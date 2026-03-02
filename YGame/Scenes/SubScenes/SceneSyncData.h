#pragma once

#include <string>
#include <vector>
#include <json.hpp>
#include "Vector3.h"
#include "SceneDataStructures.h"

///=============================================================================================
/// シーン間データ同期管理クラス
/// JSONによるデータ保存・読み込みを行い、バトルとフィールド間の状態を共有する。
///=============================================================================================

class SceneSyncData {
public:
	///************************* シングルトンパターン *************************///
	static SceneSyncData* GetInstance() {
		static SceneSyncData instance;
		return &instance;
	}

	///************************* JSON保存・読み込み *************************///

	// バトル遷移データをJSONに保存
	void SaveBattleTransitionData(const BattleTransitionData& data);

	// バトル遷移データをJSONから読み込み
	BattleTransitionData LoadBattleTransitionData();

	// フィールド復帰データをJSONに保存
	void SaveFieldReturnData(const FieldReturnData& data);

	// フィールド復帰データをJSONから読み込み
	FieldReturnData LoadFieldReturnData();

	// 現在のシーン状態を保存
	void SaveCurrentSceneState(const std::string& sceneName, const nlohmann::json& customData);

	// 現在のシーン状態を読み込み
	nlohmann::json LoadCurrentSceneState(const std::string& sceneName);

	// 同期データをクリア
	void ClearSyncData();

	// バトル遷移データを削除（使用後に呼ぶ）
	void ClearBattleTransitionData();

	// フィールド復帰データを削除（使用後に呼ぶ）
	void ClearFieldReturnData();

	///************************* アクセッサ *************************///

	// データが存在するか確認
	bool HasBattleTransitionData() const;
	bool HasFieldReturnData() const;

	// 保存先のファイルパスを設定
	void SetSyncFilePath(const std::string& path) { syncFilePath_ = path; }

	// 保存先のファイルパスを取得
	std::string GetSyncFilePath() const { return syncFilePath_; }

private:
	///************************* 内部処理 *************************///

	SceneSyncData() = default;
	~SceneSyncData() = default;
	SceneSyncData(const SceneSyncData&) = delete;
	SceneSyncData& operator=(const SceneSyncData&) = delete;

	// CameraModeをJSONに変換
	nlohmann::json CameraModeToJson(CameraMode mode);

	// JSONをCameraModeに変換
	CameraMode JsonToCameraMode(const nlohmann::json& j);

	// JSONファイルを保存
	void SaveJsonToFile(const nlohmann::json& j, const std::string& filePath) const;

	// JSONファイルを読み込み
	nlohmann::json LoadJsonFromFile(const std::string& filePath) const;

	// ファイルが存在しない場合に作成
	void EnsureFileExists(const std::string& filePath) const;

private:
	///************************* メンバ変数 *************************///

	std::string syncFilePath_ = "Resources/Json/SceneSync/sync_data.json";
};
