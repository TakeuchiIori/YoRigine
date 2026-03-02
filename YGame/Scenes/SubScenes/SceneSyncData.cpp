#include "SceneSyncData.h"
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include "Loaders/Json/JsonConverters.h"

/// <summary>
/// バトル遷移データを保存
/// </summary>
void SceneSyncData::SaveBattleTransitionData(const BattleTransitionData& data) {
	EnsureFileExists(syncFilePath_);

	nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);

	nlohmann::json j;
	j["type"] = "BattleTransition";
	j["enemyGroup"] = data.enemyGroup;
	j["battleEnemyId"] = data.battleEnemyId;
	j["battleEnemyIds"] = data.battleEnemyIds;
	j["playerPosition"] = Vector3ToJson(data.playerPosition);
	j["cameraPosition"] = Vector3ToJson(data.cameraPosition);
	j["cameraMode"] = CameraModeToJson(data.cameraMode);
	j["isFinalBattle"] = data.isFinalBattle;
	j["totalRemainingFieldEnemies"] = data.totalRemainingFieldEnemies;
	j["timestamp"] = static_cast<int>(time(nullptr));
	j["playerHitDamage"] = data.playerHitDamage;

	fullData["battleTransition"] = j;

	SaveJsonToFile(fullData, syncFilePath_);
	OutputDebugStringA(("[SceneSyncData] Battle transition data saved: Enemy=" + data.battleEnemyId + "\n").c_str());
}

/// <summary>
/// バトル遷移データを読み込み
/// </summary>
BattleTransitionData SceneSyncData::LoadBattleTransitionData() {
	BattleTransitionData data;

	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);

		if (fullData.contains("battleTransition")) {
			nlohmann::json j = fullData["battleTransition"];
			data.enemyGroup = j.value("enemyGroup", "");
			if (j.contains("battleEnemyIds")) {
				data.battleEnemyIds = j["battleEnemyIds"].get<std::vector<std::string>>();
			}
			data.battleEnemyId = j.value("battleEnemyId", "");

			if (j.contains("playerPosition")) data.playerPosition = JsonToVector3(j["playerPosition"]);
			if (j.contains("cameraPosition")) data.cameraPosition = JsonToVector3(j["cameraPosition"]);
			if (j.contains("cameraMode")) data.cameraMode = JsonToCameraMode(j["cameraMode"]);
			if (j.contains("isFinalBattle"))
				data.isFinalBattle = j["isFinalBattle"].get<bool>();

			if (j.contains("totalRemainingFieldEnemies"))
				data.totalRemainingFieldEnemies = j["totalRemainingFieldEnemies"].get<size_t>();

			if (j.contains("playerHitDamage")) data.playerHitDamage = j["playerHitDamage"].get<float>();
			OutputDebugStringA(("[SceneSyncData] Battle transition data loaded: Enemy=" + data.battleEnemyId + "\n").c_str());
		} else {
			OutputDebugStringA("[SceneSyncData] No battle transition data found\n");
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Load error: " + std::string(e.what()) + "\n").c_str());
	}

	return data;
}

/// <summary>
/// フィールド復帰データを保存
/// </summary>
void SceneSyncData::SaveFieldReturnData(const FieldReturnData& data) {
	EnsureFileExists(syncFilePath_);

	nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);

	nlohmann::json j;
	j["type"] = "FieldReturn";
	j["playerPosition"] = Vector3ToJson(data.playerPosition);
	j["cameraPosition"] = Vector3ToJson(data.cameraPosition);
	j["cameraMode"] = CameraModeToJson(data.cameraMode);
	j["defeatedEnemyGroup"] = data.defeatedEnemyGroup;
	j["playerWon"] = data.playerWon;
	j["expGained"] = data.expGained;
	j["goldGained"] = data.goldGained;
	j["itemsGained"] = data.itemsGained;
	j["playerHpRatio"] = data.playerHpRatio;
	j["timestamp"] = static_cast<int>(time(nullptr));

	fullData["fieldReturn"] = j;

	SaveJsonToFile(fullData, syncFilePath_);
	OutputDebugStringA(("[SceneSyncData] Field return data saved: Defeated=" + data.defeatedEnemyGroup + "\n").c_str());
}

/// <summary>
/// フィールド復帰データを読み込み
/// </summary>
FieldReturnData SceneSyncData::LoadFieldReturnData() {
	FieldReturnData data;

	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);

		if (fullData.contains("fieldReturn")) {
			nlohmann::json j = fullData["fieldReturn"];

			if (j.contains("playerPosition")) data.playerPosition = JsonToVector3(j["playerPosition"]);
			if (j.contains("cameraPosition")) data.cameraPosition = JsonToVector3(j["cameraPosition"]);
			if (j.contains("cameraMode")) data.cameraMode = JsonToCameraMode(j["cameraMode"]);

			data.defeatedEnemyGroup = j.value("defeatedEnemyGroup", "");
			data.playerWon = j.value("playerWon", false);
			data.expGained = j.value("expGained", 0);
			data.goldGained = j.value("goldGained", 0);
			data.playerHpRatio = j.value("playerHpRatio", 1.0f);

			if (j.contains("itemsGained") && j["itemsGained"].is_array()) {
				data.itemsGained = j["itemsGained"].get<std::vector<std::string>>();
			}

			OutputDebugStringA(("[SceneSyncData] Field return data loaded: Won=" +
				std::string(data.playerWon ? "true" : "false") + "\n").c_str());
		} else {
			OutputDebugStringA("[SceneSyncData] No field return data found\n");
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Load error: " + std::string(e.what()) + "\n").c_str());
	}

	return data;
}

/// <summary>
/// 現在のシーン状態を保存
/// </summary>
void SceneSyncData::SaveCurrentSceneState(const std::string& sceneName, const nlohmann::json& customData) {
	EnsureFileExists(syncFilePath_);

	nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);

	nlohmann::json sceneState;
	sceneState["sceneName"] = sceneName;
	sceneState["customData"] = customData;
	sceneState["timestamp"] = static_cast<int>(time(nullptr));

	fullData["currentSceneState"] = sceneState;

	SaveJsonToFile(fullData, syncFilePath_);
	OutputDebugStringA(("[SceneSyncData] Scene state saved: " + sceneName + "\n").c_str());
}

/// <summary>
/// シーン状態を読み込み
/// </summary>
nlohmann::json SceneSyncData::LoadCurrentSceneState(const std::string& sceneName) {
	nlohmann::json result;

	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);

		if (fullData.contains("currentSceneState")) {
			nlohmann::json sceneState = fullData["currentSceneState"];
			if (sceneState.value("sceneName", "") == sceneName) {
				if (sceneState.contains("customData")) {
					result = sceneState["customData"];
				}
			}
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Load scene state error: " + std::string(e.what()) + "\n").c_str());
	}

	return result;
}

/// <summary>
/// すべての同期データを削除
/// </summary>
void SceneSyncData::ClearSyncData() {
	nlohmann::json emptyData;
	SaveJsonToFile(emptyData, syncFilePath_);
	OutputDebugStringA("[SceneSyncData] All sync data cleared\n");
}

/// <summary>
/// バトル遷移データを削除
/// </summary>
void SceneSyncData::ClearBattleTransitionData() {
	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);
		if (fullData.contains("battleTransition")) {
			fullData.erase("battleTransition");
			SaveJsonToFile(fullData, syncFilePath_);
			OutputDebugStringA("[SceneSyncData] Battle transition data cleared\n");
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Clear battle transition error: " + std::string(e.what()) + "\n").c_str());
	}
}

/// <summary>
/// フィールド復帰データを削除
/// </summary>
void SceneSyncData::ClearFieldReturnData() {
	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);
		if (fullData.contains("fieldReturn")) {
			fullData.erase("fieldReturn");
			SaveJsonToFile(fullData, syncFilePath_);
			OutputDebugStringA("[SceneSyncData] Field return data cleared\n");
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Clear field return error: " + std::string(e.what()) + "\n").c_str());
	}
}

///=============================================================================================
/// アクセッサ
///=============================================================================================

/// <summary>
/// バトル遷移データが存在するか
/// </summary>
bool SceneSyncData::HasBattleTransitionData() const {
	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);
		return fullData.contains("battleTransition");
	}
	catch (...) {
		return false;
	}
}

/// <summary>
/// フィールド復帰データが存在するか
/// </summary>
bool SceneSyncData::HasFieldReturnData() const {
	try {
		nlohmann::json fullData = LoadJsonFromFile(syncFilePath_);
		return fullData.contains("fieldReturn");
	}
	catch (...) {
		return false;
	}
}

///=============================================================================================
/// 内部ユーティリティ関数
///=============================================================================================

/// <summary>
/// カメラモード → JSON文字列
/// </summary>
nlohmann::json SceneSyncData::CameraModeToJson(CameraMode mode) {
	switch (mode) {
	case CameraMode::DEFAULT:      return "DEFAULT";
	case CameraMode::FOLLOW:       return "FOLLOW";
	case CameraMode::BATTLE_START: return "BATTLE_START";
	case CameraMode::DEBUG:        return "DEBUG";
	default:                       return "DEFAULT";
	}
}

/// <summary>
/// JSON文字列 → カメラモード
/// </summary>
CameraMode SceneSyncData::JsonToCameraMode(const nlohmann::json& j) {
	std::string modeStr = j.get<std::string>();
	if (modeStr == "FOLLOW")       return CameraMode::FOLLOW;
	if (modeStr == "BATTLE_START") return CameraMode::BATTLE_START;
	if (modeStr == "DEBUG")        return CameraMode::DEBUG;
	return CameraMode::DEFAULT;
}

/// <summary>
/// JSONデータをファイルに保存
/// </summary>
void SceneSyncData::SaveJsonToFile(const nlohmann::json& j, const std::string& filePath) const {
	try {
		std::filesystem::path path(filePath);
		std::filesystem::create_directories(path.parent_path());

		std::ofstream file(filePath);
		if (file.is_open()) {
			file << j.dump(4);
			file.close();
		} else {
			OutputDebugStringA(("[SceneSyncData] Failed to open file for writing: " + filePath + "\n").c_str());
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Save error: " + std::string(e.what()) + "\n").c_str());
	}
}

/// <summary>
/// JSONファイルを読み込み
/// </summary>
nlohmann::json SceneSyncData::LoadJsonFromFile(const std::string& filePath) const {
	nlohmann::json j;
	try {
		std::ifstream file(filePath);
		if (file.is_open()) {
			file >> j;
			file.close();
		}
	}
	catch (const std::exception& e) {
		OutputDebugStringA(("[SceneSyncData] Load error: " + std::string(e.what()) + " - Creating empty JSON\n").c_str());
	}
	return j;
}

/// <summary>
/// 指定ファイルとディレクトリの存在を保証（なければ作成）
/// </summary>
void SceneSyncData::EnsureFileExists(const std::string& filePath) const {
	std::filesystem::path path(filePath);

	if (!std::filesystem::exists(path.parent_path())) {
		std::filesystem::create_directories(path.parent_path());
		OutputDebugStringA(("[SceneSyncData] Created directory: " + path.parent_path().string() + "\n").c_str());
	}

	if (!std::filesystem::exists(path)) {
		nlohmann::json emptyJson;
		SaveJsonToFile(emptyJson, filePath);
		OutputDebugStringA(("[SceneSyncData] Created empty JSON file: " + filePath + "\n").c_str());
	}
}
