#include "FieldEnemyManager.h"
#include "Player/Player.h"
#include "MathFunc.h"
#include "Systems/GameTime/GameTime.h"
#include <Loaders/Json/JsonManager.h>
#include <Debugger/Logger.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Editor/Editor.h>

FieldEnemyManager::FieldEnemyManager() = default;

FieldEnemyManager::~FieldEnemyManager() {
	fieldEnemies_.clear();
	spawnDataMap_.clear();
	enemyDataMap_.clear();
	respawnQueue_.clear();
};

/// <summary>
/// フィールド敵マネージャーの初期化
/// </summary>
/// <param name="camera">描画に使用するカメラ</param>
void FieldEnemyManager::Initialize(Camera* camera) {
	camera_ = camera;
	fieldEnemies_.clear();
	spawnDataMap_.clear();
	enemyDataMap_.clear();
	respawnQueue_.clear();

	// デフォルト設定
	encounterCooldown_ = 0.0f;
	encounterOccurred_ = false;
	isActive_ = true;
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI(
		"フィールドエネミーエディター",
		[this]() {
			ShowEnemyEditor();
		}, "Game"
	);
#endif // _DEBUG
	// 敵のデータを読み込み
	LoadEnemyData(FieldEnemyPaths::EnemyData);
	// スポーンデータを読み込み
	LoadEnemySpawnData(FieldEnemyPaths::Spawn);
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
void FieldEnemyManager::Update() {
	UpdateRespawnTimers();

	if (!isActive_) return;

	float deltaTime = YoRigine::GameTime::GetDeltaTime();

	// エンカウントクールダウン更新
	if (encounterCooldown_ > 0.0f) {
		encounterCooldown_ -= deltaTime;
	}

	// 敵の状態更新
	UpdateEnemyStates();

	// 非アクティブな敵を削除
	CleanupInactiveEnemies();
}

/// <summary>
/// 全敵の更新処理
/// </summary>
void FieldEnemyManager::UpdateEnemyStates() {
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->Update();
		}
	}
}

/// <summary>
/// リスポーン待機中の敵を更新
/// </summary>
void FieldEnemyManager::UpdateRespawnTimers() {
	float deltaTime = YoRigine::GameTime::GetDeltaTime();

	for (auto it = respawnQueue_.begin(); it != respawnQueue_.end();) {
		it->timer -= deltaTime;

		if (it->timer <= 0.0f) {
			SpawnFieldEnemy(it->spawnData);
			it = respawnQueue_.erase(it);
		} else {
			++it;
		}
	}
}

/// <summary>
/// 非アクティブな敵をリストから削除
/// </summary>
void FieldEnemyManager::CleanupInactiveEnemies() {
	fieldEnemies_.erase(
		std::remove_if(fieldEnemies_.begin(), fieldEnemies_.end(),
			[](const std::unique_ptr<FieldEnemy>& enemy) {
				return !enemy || !enemy->IsActive();
			}),
		fieldEnemies_.end()
	);
}

/// <summary>
/// 敵がプレイヤーにエンカウントしたときに呼ばれる
/// </summary>
/// <param name="enemy">エンカウントした敵</param>
void FieldEnemyManager::OnEnemyEncounter(FieldEnemy* enemy) {
	if (!enemy || encounterOccurred_) return;

	const auto& enemyData = enemy->GetEnemyData();

	lastEncounterInfo_.enemyGroup = enemy->GetEnemyGroupName();
	lastEncounterInfo_.encounterPosition = enemy->GetPosition();
	lastEncounterInfo_.encounteredEnemy = enemy;

	lastEncounterInfo_.battleType = enemy->GetBattleType();
	lastEncounterInfo_.battleFormation = enemyData.battleFormation;
	lastEncounterInfo_.battleEnemyIds = enemy->GetBattleEnemyIds();

	if (!lastEncounterInfo_.battleEnemyIds.empty()) {
		lastEncounterInfo_.battleEnemyId = lastEncounterInfo_.battleEnemyIds[0];
	} else {
		lastEncounterInfo_.battleEnemyId = enemy->GetBattleEnemyId();
	}

	encounterOccurred_ = true;
	encounterCooldown_ = encounterCooldownDuration_;

	std::string battleInfo;
	if (lastEncounterInfo_.battleEnemyIds.size() > 1) {
		battleInfo = std::to_string(lastEncounterInfo_.battleEnemyIds.size()) + "体バトル";
		for (size_t i = 0; i < lastEncounterInfo_.battleEnemyIds.size(); ++i) {
			battleInfo += "\n  [" + std::to_string(i + 1) + "] " + lastEncounterInfo_.battleEnemyIds[i];
		}
	} else {
		battleInfo = "単体バトル: " + lastEncounterInfo_.battleEnemyId;
	}

	Logger("[FieldEnemyManager] エンカウント発生: " + lastEncounterInfo_.enemyGroup +
		"\n  " + battleInfo +
		"\n  フォーメーション: " + lastEncounterInfo_.battleFormation + "\n");

	if (encounterDetailCallback_) {
		encounterDetailCallback_(lastEncounterInfo_);
	}
}

void FieldEnemyManager::ResetEnCount()
{
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->ResetEncounterState();
		}
	}
}

/// 敵をスポーンさせる
/// </summary>
/// <param name="spawnData">スポーンデータ</param>
void FieldEnemyManager::SpawnFieldEnemy(const FieldEnemySpawnData& spawnData) {
	if (!camera_) return;

	// すでに撃破済み → 非アクティブにして終了
	if (IsEnemyDefeated(spawnData.enemyId)) {
		if (spawnDataMap_.contains(spawnData.id)) {
			spawnDataMap_[spawnData.id].isActive = false;
		}
		return;
	}

	// 既存の ID の敵がいれば削除（再生成対策）
	if (auto* existingEnemy = GetFieldEnemyById(spawnData.id)) {
		RemoveFieldEnemy(spawnData.id);
	}

	//-----------------------------------------
	// 敵データを enemyDataMap_ から取得（参照）
	//-----------------------------------------
	auto it = enemyDataMap_.find(spawnData.enemyId);
	if (it == enemyDataMap_.end()) {
		Logger("[FieldEnemyManager] エラー: enemyId '" + spawnData.enemyId + "' のデータが enemyDataMap_ に存在しません\n");
		return; // データなしは致命的 → スポーン不可
	}
	const FieldEnemyData& enemyData = it->second;

	//-----------------------------------------
	// 敵インスタンス生成
	//-----------------------------------------
	auto newEnemy = std::make_unique<FieldEnemy>();
	newEnemy->Initialize(camera_);
	newEnemy->SetPlayer(player_);
	newEnemy->SetSpawnId(spawnData.id);
	newEnemy->SetFieldEnemyManager(this);
	newEnemy->InitializeFieldData(enemyData, spawnData.position);

	//-----------------------------------------
	// マップとリストに登録
	//-----------------------------------------
	spawnDataMap_[spawnData.id] = spawnData;
	fieldEnemies_.push_back(std::move(newEnemy));

	totalEnemiesSpawned_++;

	Logger("[FieldEnemyManager] 敵を生成:\n");
}

/// <summary>
/// 特定の敵を削除
/// </summary>
void FieldEnemyManager::RemoveFieldEnemy(const std::string& id) {
	auto it = std::find_if(fieldEnemies_.begin(), fieldEnemies_.end(),
		[&id, this](const std::unique_ptr<FieldEnemy>& enemy) {
			if (!enemy) return false;
			return enemy->GetSpawnId() == id;
		});

	if (it != fieldEnemies_.end()) {
		fieldEnemies_.erase(it);
	}

	spawnDataMap_.erase(id);
}

void FieldEnemyManager::SyncEnemyInstances(const std::string& enemyId)
{
	auto it = enemyDataMap_.find(enemyId);
	if (it == enemyDataMap_.end()) return;

	const FieldEnemyData& latestData = it->second;

	for (auto& enemy : fieldEnemies_) {
		// 現在フィールドにいる敵の中で、IDが一致するものを更新
		if (enemy && enemy->GetEnemyData().enemyId == enemyId) {
			enemy->ApplyUpdatedData(latestData);
		}
	}
}

/// <summary>
/// 全敵削除
/// </summary>
void FieldEnemyManager::RemoveAllFieldEnemies() {
	fieldEnemies_.clear();
	spawnDataMap_.clear();
	respawnQueue_.clear();
}

/// <summary>
/// 撃破済み敵をクリア
/// </summary>
void FieldEnemyManager::ClearDefeatedEnemies() {
	CleanupInactiveEnemies();
}

/// <summary>
/// 全敵の有効・無効を一括設定
/// </summary>
void FieldEnemyManager::SetAllEnemiesActive(bool isActive) {
	isActive_ = isActive;
	// アクティブ状態に応じて全敵のライトを切り替える
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->SetLightActive(isActive);
		}
	}
}

/// <summary>
/// バトル終了時の処理
/// </summary>
/// <param name="enemyGroup">撃破対象グループ名</param>
/// <param name="playerWon">プレイヤー勝利したか</param>
void FieldEnemyManager::HandleBattleEnd(const std::string& enemyGroup, bool playerWon) {
	Logger("[FieldEnemyManager] バトル終了処理: " + enemyGroup + " 勝利: " + (playerWon ? "はい" : "いいえ") + "\n");

	if (playerWon) {
		for (auto& enemy : fieldEnemies_) {
			if (enemy && enemy->GetEnemyGroupName() == enemyGroup) {
				RegisterDefeatedEnemy(enemyGroup);

				for (const auto& pair : spawnDataMap_) {
					if (pair.second.enemyId == enemyGroup &&
						pair.second.respawnAfterBattle) {
						RespawnInfo respawnInfo;
						respawnInfo.spawnData = pair.second;
						respawnInfo.timer = pair.second.respawnDelay;
						respawnInfo.isWaiting = true;
						respawnQueue_.push_back(respawnInfo);

						Logger("[FieldEnemyManager] リスポーンキューに追加: " + enemyGroup +
							" 待機時間: " + std::to_string(pair.second.respawnDelay) + "秒\n");
						break;
					}
				}

				enemy->ResetEncounterState();
				enemy->Despawn();

				Logger("[FieldEnemyManager] 敵を撃破済みに設定: " + enemyGroup + "\n");
				break;
			}
		}
	} else {
		for (auto& enemy : fieldEnemies_) {
			if (enemy && enemy->GetEnemyGroupName() == enemyGroup) {
				enemy->ResetEncounterState();
				Logger("[FieldEnemyManager] 敗北後、エンカウントリセット: " + enemyGroup + "\n");
				break;
			}
		}
		encounterOccurred_ = false;
	}

	encounterOccurred_ = false;
	encounterCooldown_ = 0.0f;
	Logger("[FieldEnemyManager] バトル終了処理完了\n");
}

/// <summary>
/// 撃破済み敵を登録
/// </summary>
/// <param name="id">敵ID</param>
void FieldEnemyManager::RegisterDefeatedEnemy(const std::string& id) {
	defeatedEnemyIds_.insert(id);
}

/// <summary>
/// 指定した敵が撃破済みかを確認
/// </summary>
/// <param name="id">敵ID</param>
/// <returns>撃破済みならtrue</returns>
bool FieldEnemyManager::IsEnemyDefeated(const std::string& id) const {
	return defeatedEnemyIds_.find(id) != defeatedEnemyIds_.end();
}

/// <summary>
/// 撃破済み敵リストをクリア
/// </summary>
void FieldEnemyManager::ClearDefeatedList() {
	defeatedEnemyIds_.clear();
}

/// <summary>
/// プレイヤー情報をセット
/// </summary>
/// <param name="player">プレイヤーポインタ</param>
void FieldEnemyManager::SetPlayer(Player* player) {
	player_ = player;

	for (auto& enemy : fieldEnemies_) {
		if (enemy) {
			enemy->SetPlayer(player);
		}
	}
}

/// <summary>
/// 指定IDのフィールド敵を取得
/// </summary>
/// <param name="id">敵ID</param>
/// <returns>該当する敵へのポインタ</returns>
FieldEnemy* FieldEnemyManager::GetFieldEnemyById(const std::string& id) {
	auto it = std::find_if(fieldEnemies_.begin(), fieldEnemies_.end(),
		[&id, this](const std::unique_ptr<FieldEnemy>& enemy) {
			if (!enemy) return false;
			return enemy->GetSpawnId() == id;
		});

	return (it != fieldEnemies_.end()) ? it->get() : nullptr;
}

/// <summary>
/// 指定範囲内の敵リストを取得
/// </summary>
/// <param name="center">中心座標</param>
/// <param name="range">探索範囲</param>
/// <returns>範囲内の敵リスト</returns>
std::vector<FieldEnemy*> FieldEnemyManager::GetFieldEnemiesInRange(const Vector3& center, float range) {
	std::vector<FieldEnemy*> result;

	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			float distance = Length(enemy->GetPosition() - center);
			if (distance <= range) {
				result.push_back(enemy.get());
			}
		}
	}

	return result;
}

/// <summary>
/// アクティブな敵全体のリストを取得
/// </summary>
/// <returns>アクティブな敵リスト</returns>
std::vector<FieldEnemy*> FieldEnemyManager::GetActiveFieldEnemies() {
	std::vector<FieldEnemy*> result;

	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			result.push_back(enemy.get());
		}
	}

	return result;
}

/// <summary>
/// アクティブな敵の数を取得
/// </summary>
/// <returns>アクティブな敵数</returns>
size_t FieldEnemyManager::GetActiveEnemyCount() const {
	return std::count_if(fieldEnemies_.begin(), fieldEnemies_.end(),
		[](const std::unique_ptr<FieldEnemy>& enemy) {
			return enemy && enemy->IsActive();
		});
}

/// <summary>
/// バトルグループ数を取得（エンカウント単位でカウント）
/// </summary>
/// <returns>まだエンカウントしていないアクティブな敵グループの数</returns>
size_t FieldEnemyManager::GetActiveEncounterGroupCount() const
{
	std::unordered_set<std::string> groupSet;

	for (auto& enemy : fieldEnemies_) {
		if (enemy->IsActive()) {
			groupSet.insert(enemy->GetSpawnId());
		}
	}

	return groupSet.size();
}


/// <summary>
/// 指定の敵グループが最後のエンカウントか確認
/// </summary>
/// <param name="enemyGroup">確認する敵グループ名</param>
/// <returns>このグループ以外にアクティブな敵がいない場合true</returns>
bool FieldEnemyManager::IsLastEncounterGroup(const std::string& enemyGroup) const {
	// このグループ以外にアクティブな敵がいるか確認
	for (const auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive() &&
			enemy->GetEnemyGroupName() != enemyGroup) {
			return false; // 他のアクティブな敵が存在
		}
	}
	return true; // このグループだけが残っている
}

/// <summary>
/// 敵データをJSONファイルに保存
/// </summary>
/// <param name="filePath">保存先のファイルパス</param>
void FieldEnemyManager::SaveEnemyData(const std::string& filePath) {
	try {
		nlohmann::json json;
		json["fieldEnemies"] = nlohmann::json::array();

		for (const auto& pair : enemyDataMap_) {
			const auto& data = pair.second;
			nlohmann::json enemyJson;

			enemyJson["enemyId"] = data.enemyId;
			enemyJson["modelPath"] = data.modelPath;

			// バトルタイプ
			std::string typeStr = "Single";
			switch (data.battleType) {
			case BattleType::Group: typeStr = "Group"; break;
			case BattleType::Boss: typeStr = "Boss"; break;
			default: break;
			}
			enemyJson["battleType"] = typeStr;

			// バトル敵ID
			if (!data.battleEnemyIds.empty()) {
				enemyJson["battleEnemyIds"] = nlohmann::json::array();
				for (const auto& id : data.battleEnemyIds) {
					enemyJson["battleEnemyIds"].push_back(id);
				}
			} else {
				enemyJson["battleEnemyId"] = data.battleEnemyId;
			}

			enemyJson["battleFormation"] = data.battleFormation;

			// スケール
			enemyJson["scale"]["x"] = data.scale.x;
			enemyJson["scale"]["y"] = data.scale.y;
			enemyJson["scale"]["z"] = data.scale.z;

			// パラメータ
			enemyJson["patrolRadius"] = data.patrolRadius;
			enemyJson["patrolSpeed"] = data.patrolSpeed;
			enemyJson["chaseSpeed"] = data.chaseSpeed;
			enemyJson["chaseRange"] = data.chaseRange;
			enemyJson["returnDistance"] = data.returnDistance;

			// 移動パラメータ
			enemyJson["patrolRadius"] = data.patrolRadius;
			enemyJson["patrolSpeed"] = data.patrolSpeed;
			enemyJson["chaseSpeed"] = data.chaseSpeed;
			enemyJson["chaseRange"] = data.chaseRange;
			enemyJson["returnDistance"] = data.returnDistance;

			// 視界パラメータ
			enemyJson["viewDistance"] = data.viewDistance;
			enemyJson["viewAngle"] = data.viewAngle;
			enemyJson["noiseDetectionRange"] = data.noiseDetectionRange;

			// 回転・リアクションパラメータ
			enemyJson["rotationSpeed"] = data.rotationSpeed;
			enemyJson["alertDuration"] = data.alertDuration;
			enemyJson["searchDuration"] = data.searchDuration;
			enemyJson["searchSweepAngle"] = data.searchSweepAngle;

			// カスタムカラー
			enemyJson["useCustomColor"] = data.useCustomColor;
			if (data.useCustomColor) {
				enemyJson["modelColor"]["r"] = data.modelColor.x;
				enemyJson["modelColor"]["g"] = data.modelColor.y;
				enemyJson["modelColor"]["b"] = data.modelColor.z;
				enemyJson["modelColor"]["a"] = data.modelColor.w;
			}

			// スポットライト
			enemyJson["useSpotLight"] = data.useSpotLight;
			enemyJson["spotLightColor"]["r"] = data.spotLightColor.x;
			enemyJson["spotLightColor"]["g"] = data.spotLightColor.y;
			enemyJson["spotLightColor"]["b"] = data.spotLightColor.z;
			enemyJson["spotLightColor"]["a"] = data.spotLightColor.w;
			enemyJson["spotLightIntensity"] = data.spotLightIntensity;
			enemyJson["spotLightDecay"] = data.spotLightDecay;
			enemyJson["spotLightOffset"]["x"] = data.spotLightOffset.x;
			enemyJson["spotLightOffset"]["y"] = data.spotLightOffset.y;
			enemyJson["spotLightOffset"]["z"] = data.spotLightOffset.z;
			enemyJson["spotLightPitch"] = data.spotLightPitch;


			json["fieldEnemies"].push_back(enemyJson);
		}

		// ファイルに保存
		std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
		std::ofstream file(filePath);
		file << json.dump(4);
		file.close();

		Logger("[EnemyEditor] 敵データをファイルに保存: " + filePath + "\n");
	}
	catch (const std::exception& e) {
		Logger("[EnemyEditor] エラー: 敵データ保存失敗: " + std::string(e.what()) + "\n");
	}
}


/// <summary>
/// 敵データファイルを読み込み（未使用・拡張用）
/// </summary>
/// <param name="filePath">ファイルパス</param>
void FieldEnemyManager::LoadEnemyData(const std::string& filePath) {
	try {
		std::filesystem::path path(filePath);
		// ファイルが無かったら終了
		if (!std::filesystem::exists(path)) {
			ThrowError("[FieldEnemyManager] エラー: 敵データファイルが存在しません: " + filePath + "\n");
			return;
		}

		std::ifstream file(filePath);
		// ファイルが開けなかったときも終了
		if (!file.is_open()) {
			ThrowError("[FieldEnemyManager] エラー: 敵データファイルを開けません: " + filePath + "\n");
			return;
		}

		nlohmann::json json;
		file >> json;
		file.close();

		// JsonファイルにfiledEnemiesが無い場合も終了
		if (!json.contains("fieldEnemies") || !json["fieldEnemies"].is_array()) {
			Logger("[FieldEnemyManager] 無効な敵データ形式: fieldEnemies が見つかりません\n");
			return;
		}

		// 綺麗にしてから読み込み
		enemyDataMap_.clear();


		//------------------------------------------------------------
		//					ここから読み込み開始
		//------------------------------------------------------------

		for (auto& enemyJson : json["fieldEnemies"]) {
			FieldEnemyData data;
			data.enemyId = enemyJson.value("enemyId", "");
			data.modelPath = enemyJson.value("modelPath", "");
			data.battleFormation = enemyJson.value("battleFormation", "");

			// バトルタイプ
			std::string typeStr = enemyJson.value("battleType", "Single");
			if (typeStr == "Single") data.battleType = BattleType::Single;
			else if (typeStr == "Group") data.battleType = BattleType::Group;
			else if (typeStr == "Boss")  data.battleType = BattleType::Boss;

			// バトル敵ID
			if (enemyJson.contains("battleEnemyIds")) {
				data.battleEnemyIds = enemyJson["battleEnemyIds"].get<std::vector<std::string>>();
			} else {
				data.battleEnemyId = enemyJson.value("battleEnemyId", "");
			}

			// スケール
			if (enemyJson.contains("scale")) {
				auto s = enemyJson["scale"];
				data.scale.x = s.value("x", 1.0f);
				data.scale.y = s.value("y", 1.0f);
				data.scale.z = s.value("z", 1.0f);
			}

			// 移動パラメータ
			data.patrolRadius = enemyJson.value("patrolRadius", 10.0f);
			data.patrolSpeed = enemyJson.value("patrolSpeed", 1.0f);
			data.chaseSpeed = enemyJson.value("chaseSpeed", 2.0f);
			data.chaseRange = enemyJson.value("chaseRange", 20.0f);
			data.returnDistance = enemyJson.value("returnDistance", 30.0f);

			// 視界パラメータ（既存 JSON に無い場合はデフォルト値を使用）
			data.viewDistance = enemyJson.value("viewDistance", 15.0f);
			data.viewAngle = enemyJson.value("viewAngle", 60.0f);
			data.noiseDetectionRange = enemyJson.value("noiseDetectionRange", 8.0f);

			// 回転・リアクションパラメータ（既存 JSON に無い場合はデフォルト値を使用）
			data.rotationSpeed = enemyJson.value("rotationSpeed", 5.0f);
			data.alertDuration = enemyJson.value("alertDuration", 0.8f);
			data.searchDuration = enemyJson.value("searchDuration", 3.0f);
			data.searchSweepAngle = enemyJson.value("searchSweepAngle", 70.0f);


			// カスタムカラー
			data.useCustomColor = enemyJson.value("useCustomColor", false);
			if (data.useCustomColor && enemyJson.contains("modelColor")) {
				auto c = enemyJson["modelColor"];
				data.modelColor.x = c.value("r", 1.0f);
				data.modelColor.y = c.value("g", 1.0f);
				data.modelColor.z = c.value("b", 1.0f);
				data.modelColor.w = c.value("a", 1.0f);

			}

			// スポットライトパラメータ
			data.useSpotLight = enemyJson.value("useSpotLight", true);
			if (enemyJson.contains("spotLightColor")) {
				auto c = enemyJson["spotLightColor"];
				data.spotLightColor.x = c.value("r", 1.0f);
				data.spotLightColor.y = c.value("g", 0.2f);
				data.spotLightColor.z = c.value("b", 0.2f);
				data.spotLightColor.w = c.value("a", 1.0f);
			}
			data.spotLightIntensity = enemyJson.value("spotLightIntensity", 5.0f);
			data.spotLightDecay = enemyJson.value("spotLightDecay", 2.0f);
			if (enemyJson.contains("spotLightOffset")) {
				auto o = enemyJson["spotLightOffset"];
				data.spotLightOffset.x = o.value("x", 0.0f);
				data.spotLightOffset.y = o.value("y", 1.0f);
				data.spotLightOffset.z = o.value("z", 0.0f);
			}
			data.spotLightPitch = enemyJson.value("spotLightPitch", -0.2f);

			// map に登録
			if (!data.enemyId.empty()) {
				enemyDataMap_[data.enemyId] = data;
			}
		}


		Logger("[FieldEnemyManager] 敵データを読み込みました: " +
			std::to_string(enemyDataMap_.size()) + " 件\n");
	}
	catch (const std::exception& e) {
		Logger("[FieldEnemyManager] エラー: 敵データ読み込み失敗: " + std::string(e.what()) + "\n");
	}
}

/// <summary>
/// 敵スポーンデータを保存
/// </summary>
/// <param name="filePath">保存先パス</param>
void FieldEnemyManager::SaveEnemySpawnData(const std::string& filePath) {
	try {
		nlohmann::json json;
		json["spawnPoints"] = nlohmann::json::array();

		for (const auto& pair : spawnDataMap_) {
			const auto& data = pair.second;
			nlohmann::json spawnJson;
			spawnJson["id"] = data.id;
			spawnJson["enemyId"] = data.enemyId;
			spawnJson["position"] = { {"x", data.position.x}, {"y", data.position.y}, {"z", data.position.z} };
			spawnJson["isActive"] = data.isActive;
			spawnJson["spawnCondition"] = data.spawnCondition;
			spawnJson["respawnAfterBattle"] = data.respawnAfterBattle;
			spawnJson["respawnDelay"] = data.respawnDelay;

			json["spawnPoints"].push_back(spawnJson);
		}

		std::filesystem::path path(filePath);
		std::filesystem::create_directories(path.parent_path());

		std::ofstream file(filePath);
		if (file.is_open()) {
			file << json.dump(4);
			file.close();
			Logger("[FieldEnemyManager] スポーンデータ保存完了: " + filePath + "\n");
		}
	}
	catch (const std::exception& e) {
		Logger("[FieldEnemyManager] エラー: スポーンデータ保存失敗: " + std::string(e.what()) + "\n");
	}
}

/// <summary>
/// 敵スポーンデータを読み込み
/// </summary>
/// <param name="filePath">読み込みパス</param>
void FieldEnemyManager::LoadEnemySpawnData(const std::string& filePath) {
	try {
		std::filesystem::path path(filePath);
		std::ifstream file(filePath);
		if (!file.is_open()) {
			Logger("[FieldEnemyManager] エラー: ファイルを開けません: " + filePath + "\n");
			return;
		}

		nlohmann::json json;
		file >> json;
		file.close();

		if (!json.contains("spawnPoints") || !json["spawnPoints"].is_array()) {
			Logger("[FieldEnemyManager] エラー: 無効なスポーンデータ形式\n");
			return;
		}

		for (const auto& spawnJson : json["spawnPoints"]) {
			FieldEnemySpawnData spawnData;
			spawnData.id = spawnJson.value("id", "");
			spawnData.enemyId = spawnJson.value("enemyId", "");

			if (spawnJson.contains("position")) {
				auto posJson = spawnJson["position"];
				spawnData.position.x = posJson.value("x", 0.0f);
				spawnData.position.y = posJson.value("y", 0.0f);
				spawnData.position.z = posJson.value("z", 0.0f);
			}

			spawnData.isActive = spawnJson.value("isActive", true);
			spawnData.spawnCondition = spawnJson.value("spawnCondition", "");
			spawnData.respawnAfterBattle = spawnJson.value("respawnAfterBattle", true);
			spawnData.respawnDelay = spawnJson.value("respawnDelay", 30.0f);

			SpawnFieldEnemy(spawnData);
		}

		Logger("[FieldEnemyManager] JSONから" + std::to_string(json["spawnPoints"].size()) +
			"個のスポーンポイントを読み込みました\n");
	}
	catch (const std::exception& e) {
		Logger("[FieldEnemyManager] エラー: スポーンデータ読み込み失敗: " + std::string(e.what()) + "\n");
	}
}

/// <summary>
/// デバッグ情報をImGui上に表示
/// </summary>
void FieldEnemyManager::ShowDebugInfo() {
#ifdef USE_IMGUI
	ImGui::Text("=== フィールド敵マネージャー ===");
	ImGui::Separator();

	ImGui::Text("アクティブな敵: %d", static_cast<int>(GetActiveEnemyCount()));
	ImGui::Text("スポーンデータ: %d", static_cast<int>(spawnDataMap_.size()));
	ImGui::Text("リスポーンキュー: %d", static_cast<int>(respawnQueue_.size()));
	ImGui::Text("撃破済み敵: %d", static_cast<int>(defeatedEnemyIds_.size()));
	ImGui::Text("エンカウントクールダウン: %.2f秒", encounterCooldown_);
	ImGui::Text("エンカウント発生中: %s", encounterOccurred_ ? "はい" : "いいえ");

	ImGui::Separator();

	ImGui::Checkbox("マネージャー有効", &isActive_);

	if (ImGui::Button("全敵削除")) {
		RemoveAllFieldEnemies();
	}

	ImGui::SameLine();
	if (ImGui::Button("撃破リストクリア")) {
		ClearDefeatedList();
	}

	ImGui::SameLine();
	if (ImGui::Button("全敵エンカウントリセット")) {
		for (auto& enemy : fieldEnemies_) {
			if (enemy) {
				enemy->ResetEncounterState();
			}
		}
		encounterOccurred_ = false;
		encounterCooldown_ = 0.0f;
		Logger("[FieldEnemyManager] 全敵のエンカウント状態をリセット\n");
	}

	ImGui::Separator();
	ImGui::Text("=== 最後のエンカウント ===");
	ImGui::Text("グループ: %s", lastEncounterInfo_.enemyGroup.c_str());
	ImGui::Text("バトルID: %s", lastEncounterInfo_.battleEnemyId.c_str());

	if (lastEncounterInfo_.encounteredEnemy) {
		Vector3 pos = lastEncounterInfo_.encounterPosition;
		ImGui::Text("発生位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
	}

	ImGui::Separator();

	// 各敵情報の一覧表示
	if (ImGui::TreeNode("アクティブな敵一覧")) {
		int enemyIndex = 0;
		for (auto& enemy : fieldEnemies_) {
			if (enemy && enemy->IsActive()) {
				const auto& data = enemy->GetEnemyData();
				std::string label = "[" + std::to_string(enemyIndex) + "] " + data.enemyId;

				bool canEncounter = enemy->CanTriggerEncounter();
				if (!canEncounter) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
				}

				if (ImGui::TreeNode(label.c_str())) {
					ImGui::Text("敵ID: %s", data.enemyId.c_str());
					ImGui::Text("バトルID: %s", data.battleEnemyId.c_str());
					ImGui::Text("モデル: %s", data.modelPath.c_str());

					ImGui::Separator();

					Vector3 pos = enemy->GetPosition();
					ImGui::Text("現在位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

					Vector3 spawnPos = enemy->GetSpawnPosition();
					ImGui::Text("スポーン位置: (%.1f, %.1f, %.1f)", spawnPos.x, spawnPos.y, spawnPos.z);

					float distanceFromSpawn = Length(pos - spawnPos);
					ImGui::Text("スポーンからの距離: %.1f", distanceFromSpawn);

					ImGui::Separator();
					const char* stateNames[] = { "巡回", "追跡", "消滅" };
					ImGui::Text("状態: %s", stateNames[static_cast<int>(enemy->GetLogicalState())]);
					ImGui::Text("状態時間: %.2f秒", enemy->GetStateTimer());

					ImGui::Separator();

					ImGui::Text("=== エンカウント情報 ===");
					ImGui::Text("エンカウント済み: %s", enemy->HasTriggeredEncounter() ? "はい" : "いいえ");
					ImGui::Text("エンカウント可能: %s", canEncounter ? "はい" : "いいえ");
					ImGui::Text("クールダウン: %.2f秒", enemy->GetEncounterCooldown());

					if (ImGui::Button("エンカウントリセット")) {
						enemy->ResetEncounterState();
						Logger("[FieldEnemyManager] エンカウントリセット: " + data.enemyId + "\n");
					}
					ImGui::Separator();
					if (ImGui::Button("この敵を削除")) {
						enemy->Despawn();
					}

					ImGui::SameLine();
					if (ImGui::Button("スポーン位置に戻す")) {
						enemy->SetTranslate(spawnPos);
						enemy->ResetStateTimer();
					}

					ImGui::TreePop();
				}

				if (!canEncounter) {
					ImGui::PopStyleColor();
				}

				enemyIndex++;
			}
		}
		ImGui::TreePop();
	}

	ImGui::Separator();

	if (!respawnQueue_.empty() && ImGui::TreeNode("リスポーンキュー")) {
		for (size_t i = 0; i < respawnQueue_.size(); ++i) {
			const auto& respawn = respawnQueue_[i];
			ImGui::Text("[%d] %s - %.1f秒後",
				static_cast<int>(i),
				respawn.spawnData.enemyId.c_str(),
				respawn.timer);
		}
		ImGui::TreePop();
	}

	if (!defeatedEnemyIds_.empty() && ImGui::TreeNode("撃破済み敵")) {
		for (const auto& id : defeatedEnemyIds_) {
			ImGui::Text("- %s", id.c_str());
		}
		ImGui::TreePop();
	}
#endif
}

/// <summary>
/// 全敵の描画
/// </summary>
void FieldEnemyManager::Draw() {
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->Draw();
		}
	}
}

void FieldEnemyManager::DrawShadow()
{
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->DrawShadow();
		}
	}
}

/// <summary>
/// 敵コリジョンの描画
/// </summary>
void FieldEnemyManager::DrawCollision() {
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->DrawCollision();
		}
	}
}

void FieldEnemyManager::DrawLine(Line* line)
{
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->DrawLine(line);
		}
	}
}

void FieldEnemyManager::DrawUI()
{
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->DrawUI();
		}
	}
}

/// <summary>
/// 終了処理（全データのクリア）
/// </summary>
void FieldEnemyManager::Finalize() {
	RemoveAllFieldEnemies();
	enemyDataMap_.clear();
	defeatedEnemyIds_.clear();
}

/// <summary>
/// エディターウィンドウの表示（ImGui）
/// </summary>
void FieldEnemyManager::ShowEnemyEditor() {
#ifdef USE_IMGUI
	ImGui::Text("=== エネミーエディター ===");
	ImGui::Separator();

	// タブの切り替え
	if (ImGui::BeginTabBar("EnemyEditorTabs")) {

		// 敵データタブ
		if (ImGui::BeginTabItem("敵データ")) {
			ShowEnemyDataEditor();
			ImGui::EndTabItem();
		}

		// スポーンポイントタブ
		if (ImGui::BeginTabItem("スポーンポイント")) {
			ShowSpawnPointEditor();
			ImGui::EndTabItem();
		}

		// プレビューデータタブ
		if (ImGui::BeginTabItem("プレビュー")) {
			ImGui::Text("現在の敵一覧");
			ImGui::Separator();

			for (const auto& pair : enemyDataMap_) {
				const auto& data = pair.second;
				ImGui::Text("ID: %s", data.enemyId.c_str());
				ImGui::Text("  タイプ: %s", data.GetBattleTypeString().c_str());
				ImGui::Text("  モデル: %s", data.modelPath.c_str());

				if (!data.battleEnemyIds.empty()) {
					ImGui::Text("  バトル敵: %d体", static_cast<int>(data.battleEnemyIds.size()));
					for (const auto& id : data.battleEnemyIds) {
						ImGui::Text("    - %s", id.c_str());
					}
				} else {
					ImGui::Text("  バトル敵: %s", data.battleEnemyId.c_str());
				}
				ImGui::Separator();
			}
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

#endif
}

/// <summary>
/// 敵データエディターの表示
/// </summary>
void FieldEnemyManager::ShowEnemyDataEditor() {
#ifdef USE_IMGUI
	ImGui::Text("=== 敵データエディター ===");
	ImGui::Separator();

	if (ImGui::Button("新しい敵データを作成", ImVec2(200, 30))) {
		CreateNewEnemyData();
	}

	ImGui::Separator();
	ImGui::Text("既存の敵データ:");

	static char searchBuffer[256] = "";
	ImGui::InputText("検索", searchBuffer, sizeof(searchBuffer));

	ImGui::BeginChild("EnemyDataList", ImVec2(0, 300), true);
	for (auto& pair : enemyDataMap_) {
		const std::string& id = pair.first;
		if (strlen(searchBuffer) > 0 && id.find(searchBuffer) == std::string::npos) continue;

		bool isSelected = (selectedEnemyId_ == id);
		if (ImGui::Selectable(id.c_str(), isSelected)) {
			selectedEnemyId_ = id;
			editorEnemyData_ = pair.second;
		}
	}
	ImGui::EndChild();

	if (!selectedEnemyId_.empty()) {
		ImGui::Separator();
		ImGui::Text("編集中: %s", selectedEnemyId_.c_str());

		// ★ 変更検知フラグ
		bool changed = false;

		ImGui::Text("=== 基本情報 ===");
		static char modelPathBuffer[256];
		strcpy_s(modelPathBuffer, editorEnemyData_.modelPath.c_str());
		if (ImGui::InputText("モデルパス", modelPathBuffer, sizeof(modelPathBuffer))) {
			editorEnemyData_.modelPath = modelPathBuffer;
			changed = true;
		}

		const char* battleTypes[] = { "単体", "グループ", "ボス" };
		int currentType = static_cast<int>(editorEnemyData_.battleType);
		if (ImGui::Combo("バトルタイプ", &currentType, battleTypes, 3)) {
			editorEnemyData_.battleType = static_cast<BattleType>(currentType);
			changed = true;
		}

		changed |= ImGui::DragFloat3("スケール", &editorEnemyData_.scale.x, 0.1f, 0.1f, 10.0f);

		ImGui::Separator();
		ImGui::Text("=== バトル設定 ===");
		if (editorEnemyData_.battleType == BattleType::Single) {
			static char battleIdBuffer[256];
			strcpy_s(battleIdBuffer, editorEnemyData_.battleEnemyId.c_str());
			if (ImGui::InputText("バトル敵ID", battleIdBuffer, sizeof(battleIdBuffer))) {
				editorEnemyData_.battleEnemyId = battleIdBuffer;
				changed = true;
			}
		} else {
			ImGui::Text("バトル敵IDリスト:");
			for (size_t i = 0; i < editorEnemyData_.battleEnemyIds.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));
				ImGui::Text("%d: %s", static_cast<int>(i + 1), editorEnemyData_.battleEnemyIds[i].c_str());
				ImGui::SameLine();
				if (ImGui::Button("削除")) {
					editorEnemyData_.battleEnemyIds.erase(editorEnemyData_.battleEnemyIds.begin() + i);
					changed = true;
				}
				ImGui::PopID();
			}

			static char newBattleIdBuffer[256] = "";
			ImGui::InputText("新しいバトル敵ID", newBattleIdBuffer, sizeof(newBattleIdBuffer));
			ImGui::SameLine();
			if (ImGui::Button("追加") && strlen(newBattleIdBuffer) > 0) {
				editorEnemyData_.battleEnemyIds.push_back(newBattleIdBuffer);
				newBattleIdBuffer[0] = '\0';
				changed = true;
			}
		}

		static char formationBuffer[256];
		strcpy_s(formationBuffer, editorEnemyData_.battleFormation.c_str());
		if (ImGui::InputText("バトルフォーメーション", formationBuffer, sizeof(formationBuffer))) {
			editorEnemyData_.battleFormation = formationBuffer;
			changed = true;
		}

		ImGui::Separator();
		ImGui::Text("=== 移動パラメータ ===");
		changed |= ImGui::DragFloat("巡回半径", &editorEnemyData_.patrolRadius, 0.5f, 0.0f, 50.0f);
		changed |= ImGui::DragFloat("巡回速度", &editorEnemyData_.patrolSpeed, 0.1f, 0.1f, 20.0f);
		changed |= ImGui::DragFloat("追跡速度", &editorEnemyData_.chaseSpeed, 0.1f, 0.1f, 20.0f);
		changed |= ImGui::DragFloat("追跡範囲", &editorEnemyData_.chaseRange, 0.5f, 1.0f, 50.0f);
		changed |= ImGui::DragFloat("帰還距離", &editorEnemyData_.returnDistance, 0.5f, 1.0f, 50.0f);

		ImGui::Separator();
		ImGui::Text("=== 視界・索敵パラメータ ===");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("DrawViewCone の描画と完全に連動します");
		}
		changed |= ImGui::DragFloat("視界距離", &editorEnemyData_.viewDistance, 0.5f, 1.0f, 100.0f);
		changed |= ImGui::DragFloat("視野角 (度)", &editorEnemyData_.viewAngle, 1.0f, 1.0f, 180.0f);
		changed |= ImGui::DragFloat("足音検知範囲", &editorEnemyData_.noiseDetectionRange, 0.5f, 1.0f, 50.0f);

		ImGui::Separator();
		ImGui::Text("=== 回転・リアクションパラメータ ===");
		ImGui::SetNextItemWidth(200.0f);
		changed |= ImGui::DragFloat("補間回転速度 (rad/s)", &editorEnemyData_.rotationSpeed, 0.1f, 0.5f, 30.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("(大きいほど素早く向く)");

		changed |= ImGui::DragFloat("発見リアクション時間 (秒)", &editorEnemyData_.alertDuration, 0.05f, 0.1f, 3.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("！停止時間");

		changed |= ImGui::DragFloat("索敵継続時間 (秒)", &editorEnemyData_.searchDuration, 0.1f, 0.5f, 10.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("？首振り時間");

		changed |= ImGui::DragFloat("索敵スウィープ角 (度)", &editorEnemyData_.searchSweepAngle, 1.0f, 10.0f, 120.0f);
		ImGui::SameLine();
		ImGui::TextDisabled("左右に振れる角度");

		ImGui::Separator();
		ImGui::Text("=== 見た目設定 ===");
		changed |= ImGui::Checkbox("カスタムカラーを使用", &editorEnemyData_.useCustomColor);
		if (editorEnemyData_.useCustomColor) {
			changed |= ImGui::ColorEdit4("モデルカラー", &editorEnemyData_.modelColor.x);
		}

		ImGui::Separator();
		ImGui::Text("=== スポットライト(視界)設定 ===");
		changed |= ImGui::Checkbox("スポットライトを使用", &editorEnemyData_.useSpotLight);
		if (editorEnemyData_.useSpotLight) {
			changed |= ImGui::ColorEdit4("ライトカラー", &editorEnemyData_.spotLightColor.x);
			changed |= ImGui::DragFloat("強度", &editorEnemyData_.spotLightIntensity, 0.1f, 0.0f, 1000.0f);
			changed |= ImGui::DragFloat("減衰(Decay)", &editorEnemyData_.spotLightDecay, 0.1f, 0.0f, 10.0f);
			changed |= ImGui::DragFloat3("位置オフセット", &editorEnemyData_.spotLightOffset.x, 0.1f);
			changed |= ImGui::DragFloat("ピッチ(下向き加減)", &editorEnemyData_.spotLightPitch, 0.01f, -1.0f, 1.0f);
		}


		// ★★★ リアルタイム反映処理 ★★★
		if (changed) {
			// 1. マスターデータを更新
			enemyDataMap_[selectedEnemyId_] = editorEnemyData_;

			// 2. 現在出現中の同じIDを持つ敵すべてに反映
			for (auto& enemy : fieldEnemies_) {
				if (enemy && enemy->GetEnemyData().enemyId == selectedEnemyId_) {
					enemy->ApplyUpdatedData(editorEnemyData_);
				}
			}
		}

		ImGui::Separator();
		if (ImGui::Button("変更を保存(JSON)", ImVec2(120, 30))) {
			SaveEnemyData(FieldEnemyPaths::EnemyData);
			Logger("[EnemyEditor] 敵データをファイルに保存しました\n");
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120, 30))) {
			editorEnemyData_ = enemyDataMap_[selectedEnemyId_];
			// キャンセル時も念のため再同期して戻す
			for (auto& enemy : fieldEnemies_) {
				if (enemy && enemy->GetEnemyData().enemyId == selectedEnemyId_) {
					enemy->ApplyUpdatedData(editorEnemyData_);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("削除", ImVec2(120, 30))) {
			DeleteEnemyData(selectedEnemyId_);
		}
	}
#endif
}

/// <summary>
/// スポーンポイントエディターの表示
/// </summary>
void FieldEnemyManager::ShowSpawnPointEditor() {
#ifdef USE_IMGUI
	ImGui::Text("=== スポーンポイントエディター ===");
	ImGui::Separator();

	if (ImGui::Button("新しいスポーンポイントを作成", ImVec2(220, 30))) {
		CreateNewSpawnPoint();
	}

	ImGui::Separator();
	ImGui::Text("スポーンポイント一覧:");

	ImGui::BeginChild("SpawnPointList", ImVec2(0, 250), true);
	for (auto& pair : spawnDataMap_) {
		const std::string& id = pair.first;
		const auto& spawn = pair.second;
		bool isSelected = (selectedSpawnId_ == id);

		std::string label = id + " (" + spawn.enemyId + ")";
		if (ImGui::Selectable(label.c_str(), isSelected)) {
			selectedSpawnId_ = id;
			editorSpawnData_ = spawn;
		}
	}
	ImGui::EndChild();

	if (!selectedSpawnId_.empty()) {
		ImGui::Separator();
		ImGui::Text("編集中: %s", selectedSpawnId_.c_str());
		ImGui::Text("=== 基本設定 ===");

		if (ImGui::BeginCombo("敵ID", editorSpawnData_.enemyId.c_str())) {
			for (const auto& pair : enemyDataMap_) {
				bool isSelected = (pair.first == editorSpawnData_.enemyId);
				if (ImGui::Selectable(pair.first.c_str(), isSelected)) {
					editorSpawnData_.enemyId = pair.first;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::DragFloat3("位置", &editorSpawnData_.position.x, 0.5f);
		if (player_ && ImGui::Button("プレイヤーの位置に配置")) {
			editorSpawnData_.position = player_->GetWorldPosition();
		}

		ImGui::Separator();
		ImGui::Text("=== リスポーン設定 ===");
		ImGui::Checkbox("アクティブ", &editorSpawnData_.isActive);
		ImGui::Checkbox("バトル後にリスポーン", &editorSpawnData_.respawnAfterBattle);
		if (editorSpawnData_.respawnAfterBattle) {
			ImGui::DragFloat("リスポーン遅延(秒)", &editorSpawnData_.respawnDelay, 1.0f, 0.0f, 300.0f);
		}

		static char conditionBuffer[256];
		strcpy_s(conditionBuffer, editorSpawnData_.spawnCondition.c_str());
		if (ImGui::InputText("スポーン条件", conditionBuffer, sizeof(conditionBuffer))) {
			editorSpawnData_.spawnCondition = conditionBuffer;
		}

		static char commentBuffer[512];
		strcpy_s(commentBuffer, editorSpawnData_.comment.c_str());
		if (ImGui::InputTextMultiline("コメント", commentBuffer, sizeof(commentBuffer))) {
			editorSpawnData_.comment = commentBuffer;
		}

		ImGui::Separator();
		ImGui::Checkbox("エディター専用", &editorSpawnData_.isEditorOnly);
		if (editorSpawnData_.isEditorOnly) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "※ゲーム実行時には表示されません");
		}

		ImGui::Separator();
		if (ImGui::Button("変更を保存", ImVec2(120, 30))) {
			spawnDataMap_[selectedSpawnId_] = editorSpawnData_;
			SaveEnemySpawnData(FieldEnemyPaths::Spawn);
			Logger("[EnemyEditor] スポーンポイントを保存: " + selectedSpawnId_ + "\n");
		}

		ImGui::SameLine();
		if (ImGui::Button("即座にスポーン", ImVec2(120, 30))) {
			SpawnFieldEnemy(editorSpawnData_);
		}

		ImGui::SameLine();
		if (ImGui::Button("削除", ImVec2(120, 30))) {
			DeleteSpawnPoint(selectedSpawnId_);
		}
	}
#endif
}

/// <summary>
/// 新しい敵データを作成
/// </summary>
void FieldEnemyManager::CreateNewEnemyData() {
	static int newEnemyCounter = 0;
	newEnemyCounter++;

	std::string newId = "NewEnemy_" + std::to_string(newEnemyCounter);

	FieldEnemyData newData;
	newData.enemyId = newId;
	newData.modelPath = "default_enemy.obj";
	newData.battleEnemyId = "alien";
	newData.battleType = BattleType::Single;

	enemyDataMap_[newId] = newData;
	selectedEnemyId_ = newId;
	editorEnemyData_ = newData;

	Logger("[EnemyEditor] 新しい敵データを作成: " + newId + "\n");
}

/// <summary>
/// 指定された敵データを編集対象として選択
/// </summary>
/// <param name="enemyId">編集する敵データのID</param>
void FieldEnemyManager::EditEnemyData(const std::string& enemyId) {
	auto it = enemyDataMap_.find(enemyId);
	if (it != enemyDataMap_.end()) {
		selectedEnemyId_ = enemyId;
		editorEnemyData_ = it->second;
	}
}

/// <summary>
/// 指定された敵データを削除
/// </summary>
/// <param name="enemyId">削除する敵データのID</param>
void FieldEnemyManager::DeleteEnemyData(const std::string& enemyId) {
	enemyDataMap_.erase(enemyId);
	selectedEnemyId_.clear();

	SaveEnemyData(FieldEnemyPaths::EnemyData);
	Logger("[EnemyEditor] 敵データを削除: " + enemyId + "\n");
}

/// <summary>
/// 新しいスポーンポイントを作成
/// </summary>
void FieldEnemyManager::CreateNewSpawnPoint() {
	static int newSpawnCounter = 0;
	newSpawnCounter++;

	std::string newId = "Spawn_" + std::to_string(newSpawnCounter);

	FieldEnemySpawnData newSpawn;
	newSpawn.id = newId;
	newSpawn.enemyId = enemyDataMap_.empty() ? "alien" : enemyDataMap_.begin()->first;
	newSpawn.position = Vector3(0.0f, 0.0f, 0.0f);
	newSpawn.isActive = true;

	spawnDataMap_[newId] = newSpawn;
	selectedSpawnId_ = newId;
	editorSpawnData_ = newSpawn;

	Logger("[EnemyEditor] 新しいスポーンポイントを作成: " + newId + "\n");
}

/// <summary>
/// 指定されたスポーンポイントを編集対象として選択
/// </summary>
/// <param name="spawnId">編集するスポーンポイントのID</param>
void FieldEnemyManager::EditSpawnPoint(const std::string& spawnId) {
	auto it = spawnDataMap_.find(spawnId);
	if (it != spawnDataMap_.end()) {
		selectedSpawnId_ = spawnId;
		editorSpawnData_ = it->second;
	}
}

/// <summary>
/// 指定されたスポーンポイントを削除
/// </summary>
/// <param name="spawnId">削除するスポーンポイントのID</param>
void FieldEnemyManager::DeleteSpawnPoint(const std::string& spawnId) {
	// 実際にスポーンされている敵も削除
	RemoveFieldEnemy(spawnId);

	spawnDataMap_.erase(spawnId);
	selectedSpawnId_.clear();

	SaveEnemySpawnData(FieldEnemyPaths::Spawn);
	Logger("[EnemyEditor] スポーンポイントを削除: " + spawnId + "\n");
}

/// <summary>
/// スポーンポイントのギズモ描画
/// </summary>
void FieldEnemyManager::DrawEditorGizmos() {
#ifdef USE_IMGUI
	if (!isEditorMode_) return;
	// TODO: ギズモ可視化処理（ライン描画など）
#endif
}