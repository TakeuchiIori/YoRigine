#include "FieldEnemyManager.h"
#include "Player/Player.h"
#include "MathFunc.h"
#include "Systems/GameTime/GameTime.h"
#include <Loaders/Json/JsonManager.h>
#include <Loaders/Json/Use/AutoJson.h>
#include <Debugger/Logger.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "SpawnPointGizmable.h"
#include "Drawer/LineManager/Line.h"
#include "FieldEnemyEditorUI.h"
#endif
#include <Editor/Editor.h>
#include <Drawer/InstancedObject3d.h>
#include "Object3D/BaseObjectManager.h"

namespace {
	// BaseObjectManager 登録名の一意連番（プロセス内で単調増加）
	uint32_t g_fieldEnemyRegSeq = 0;
}

FieldEnemyManager::FieldEnemyManager() = default;

FieldEnemyManager::~FieldEnemyManager() {
#ifdef USE_IMGUI
	if (gizmoCallbackId_ != 0) {
		Editor::GetInstance()->RemoveGizmoDrawCallback(gizmoCallbackId_);
		gizmoCallbackId_ = 0;
	}
#endif
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
			FieldEnemyEditorUI::ShowEnemyEditor(*this);
		}, "Game"
	);
	spawnGizmoCtrl_.Initialize();
	// ImGuizmo はゲームビュー ImGui ウィンドウのコンテキスト内で描画する必要があるため、
	// Editor のギズモコールバックに登録する (FieldScene の DrawLine ではダメ)。
	gizmoCallbackId_ = Editor::GetInstance()->AddGizmoDrawCallback([this]() {
		DrawSpawnPointGizmoHandles();
	});

	// スポーンマーカー用 Line インスタンス (状態別に 4 本)。
	// 各々が独立した GPU バッファを持つので、DrawLine 1 回ずつで stomp が起きない。
	markerLineSelected_ = std::make_unique<Line>();
	markerLineActive_   = std::make_unique<Line>();
	markerLineInactive_ = std::make_unique<Line>();
	markerLinePole_     = std::make_unique<Line>();
	markerLineSelected_->Initialize();
	markerLineActive_  ->Initialize();
	markerLineInactive_->Initialize();
	markerLinePole_    ->Initialize();
	markerLineSelected_->SetCamera(camera_);
	markerLineActive_  ->SetCamera(camera_);
	markerLineInactive_->SetCamera(camera_);
	markerLinePole_    ->SetCamera(camera_);
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
		}
		else {
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
	lastEncounterInfo_.encounterScale = enemy->GetScale();

	if (!lastEncounterInfo_.battleEnemyIds.empty()) {
		lastEncounterInfo_.battleEnemyId = lastEncounterInfo_.battleEnemyIds[0];
	}
	else {
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
	}
	else {
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

/// <summary>
/// 登録済みスポーンポイントのうち、まだ実体が出ていないものを一気にスポーン
/// </summary>
void FieldEnemyManager::SpawnAllPending() {
	int spawned = 0;
	int skipped = 0;
	// SpawnFieldEnemy は内部で spawnDataMap_ を書き換える可能性があるため、
	// イテレーションする前にスナップショットを取る。
	std::vector<FieldEnemySpawnData> snapshot;
	snapshot.reserve(spawnDataMap_.size());
	for (const auto& [id, data] : spawnDataMap_) {
		snapshot.push_back(data);
	}
	for (const auto& data : snapshot) {
		// すでに実体が出ているスポーンはスキップ
		if (GetFieldEnemyById(data.id) != nullptr) {
			++skipped;
			continue;
		}
		// SpawnFieldEnemy は撃破済 / enemyData 未定義などの場合 early return する
		SpawnFieldEnemy(data);
		// 上の呼び出しで実体が増えていれば成功
		if (GetFieldEnemyById(data.id) != nullptr) ++spawned;
	}
	Logger("[FieldEnemyManager] SpawnAllPending: spawned=" + std::to_string(spawned)
		+ " skipped(already alive)=" + std::to_string(skipped) + "\n");
}

/// <summary>
/// 現在フィールドにいる敵を全部消す (spawnDataMap_ は維持)
/// </summary>
void FieldEnemyManager::DespawnAll() {
	const int killed = static_cast<int>(fieldEnemies_.size());
	fieldEnemies_.clear();
	respawnQueue_.clear();
	encounterOccurred_ = false;
	encounterCooldown_ = 0.0f;
	Logger("[FieldEnemyManager] DespawnAll: removed " + std::to_string(killed) + " enemies\n");
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
	newEnemy->SetNavPathfinder(navPathfinder_); // NavPathfinder を注入
	newEnemy->InitializeFieldData(enemyData, spawnData.position);

	//-----------------------------------------
	// マップとリストに登録
	//-----------------------------------------
	spawnDataMap_[spawnData.id] = spawnData;
	fieldEnemies_.push_back(std::move(newEnemy));

	// BaseObjectManager へ一意名で登録（解除は FieldEnemy デストラクタが担当）
	BaseObjectManager::GetInstance()->Register(
		fieldEnemies_.back().get(),
		"FieldEnemy_" + std::to_string(g_fieldEnemyRegSeq++));

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

std::string FieldEnemyManager::GenerateUniqueSpawnPointId(const std::string& prefix) const {
	const std::string base = prefix.empty() ? "Spawn" : prefix;
	for (int i = 1; i < 100000; ++i) {
		const std::string candidate = base + "_" + std::to_string(i);
		if (!spawnDataMap_.contains(candidate)) {
			return candidate;
		}
	}

	return base + "_overflow";
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
	// アクティブ状態に応じて全敵のライトとコライダーを切り替える。
	// コライダーを落とさないと BattleScene 中も FieldEnemy の OBB が
	// グローバル CollisionManager にヒットしてプレイヤーの移動を塞ぐ。
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->SetLightActive(isActive);
			enemy->SetCollisionActive(isActive);
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
				// リスポーン対象の敵は defeatedEnemyIds_ に登録しない。
				// 登録すると SpawnFieldEnemy 冒頭の IsEnemyDefeated チェックで
				// 再スポーンが永久ブロックされ、EventTrigger のカウントが 1 で止まる。
				bool willRespawn = false;
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
						willRespawn = true;
						break;
					}
				}

				if (willRespawn) {
					if (onEnemyDefeatedCallback_) {
						onEnemyDefeatedCallback_(enemyGroup);
					}
				} else {
					RegisterDefeatedEnemy(enemyGroup);
				}

				enemy->ResetEncounterState();
				enemy->Despawn();

				Logger("[FieldEnemyManager] 敵を撃破済みに設定: " + enemyGroup + "\n");
				break;
			}
		}
	}
	else {
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
	if (onEnemyDefeatedCallback_) {
		onEnemyDefeatedCallback_(id);
	}
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

// =============================================================================
// BuildFieldEnemyDataAj
// FieldEnemyData の全フィールドを AutoJson に登録して返す。
//
// FieldEnemyData は unordered_map に格納されたり FieldEnemy::enemyData_ に
// コピーされたりするためコピー可能である必要がある。
// AutoJson は unique_ptr を内包するためコピー不可。
// → FieldEnemyData のメンバに持たせず、I/O 時に一時生成する方式を採用。
//
// 注意: 返した AutoJson は第一引数の data の生存期間内に使うこと
//      （AddGroup のポインタが data のメンバを指しているため）。
// =============================================================================
static AutoJson BuildFieldEnemyDataAj(FieldEnemyData& d)
{
	AutoJson aj;
	aj.Add("enemyId", &d.enemyId)
		.Add("modelPath", &d.modelPath)
		.Add("battleEnemyId", &d.battleEnemyId)
		.Add("battleFormation", &d.battleFormation)
		.Add("battleType", &d.battleType)
		.Add("scale", &d.scale)
		.Add("patrolRadius", &d.patrolRadius)
		.Add("patrolSpeed", &d.patrolSpeed)
		.Add("chaseSpeed", &d.chaseSpeed)
		.Add("chaseRange", &d.chaseRange)
		.Add("returnDistance", &d.returnDistance)
		.Add("pathRefreshInterval", &d.pathRefreshInterval)
		.Add("viewDistance", &d.viewDistance)
		.Add("viewAngle", &d.viewAngle)
		.Add("noiseDetectionRange", &d.noiseDetectionRange)
		.Add("rotationSpeed", &d.rotationSpeed)
		.Add("alertDuration", &d.alertDuration)
		.Add("searchDuration", &d.searchDuration)
		.Add("searchSweepAngle", &d.searchSweepAngle)
		.Add("useCustomColor", &d.useCustomColor)
		.Add("modelColor", &d.modelColor)
		.Add("useSpotLight", &d.useSpotLight)
		.Add("spotLightColor", &d.spotLightColor)
		.Add("spotLightIntensity", &d.spotLightIntensity)
		.Add("spotLightDecay", &d.spotLightDecay)
		.Add("spotLightOffset", &d.spotLightOffset)
		.Add("spotLightPitch", &d.spotLightPitch);
	return aj;
}

void FieldEnemyManager::SaveEnemyData(const std::string& filePath) {
	try {
		// 各敵に対して一時AutoJsonを作り AddGroup でネスト登録
		//    ajList を root より先に宣言することで、root の SaveToFile 中も
		//    ポインタが有効（ajList は root より後に破棄される）
		std::vector<AutoJson> ajList;
		ajList.reserve(enemyDataMap_.size());

		AutoJson root;
		for (auto& [id, data] : enemyDataMap_) {
			auto& aj = ajList.emplace_back(BuildFieldEnemyDataAj(data));
			root.AddGroup(id, aj);
		}

		std::filesystem::create_directories(
			std::filesystem::path(filePath).parent_path());
		root.SaveToFile(filePath);

		// ③ battleEnemyIds を追記（vector<string> は AutoJson 未対応）
		if (!enemyDataMap_.empty()) {
			std::ifstream ifs(filePath);
			nlohmann::json j;
			ifs >> j;
			ifs.close();
			for (const auto& [id, data] : enemyDataMap_) {
				if (!data.battleEnemyIds.empty()) {
					j[id]["battleEnemyIds"] = data.battleEnemyIds;
				}
			}
			std::ofstream ofs(filePath);
			ofs << j.dump(4);
		}

		Logger("[EnemyEditor] 敵データをファイルに保存: " + filePath + "\n");
	}
	catch (const std::exception& e) {
		Logger("[EnemyEditor] エラー: 敵データ保存失敗: " + std::string(e.what()) + "\n");
	}
}

// ============================================================
// 敵データファイルを読み込み（全エントリ正しく反映）
// ============================================================

void FieldEnemyManager::LoadEnemyData(const std::string& filePath) {
	try {
		std::filesystem::path path(filePath);
		// ファイル存在チェック
		if (!std::filesystem::exists(path)) {
			ThrowError("[FieldEnemyManager] エラー: 敵データファイルが存在しません: " + filePath + "\n");
			return;
		}

		std::ifstream file(filePath);
		if (!file.is_open()) {
			ThrowError("[FieldEnemyManager] エラー: 敵データファイルを開けません: " + filePath + "\n");
			return;
		}

		nlohmann::json json;
		file >> json; // JSONロード
		file.close();

		if (json.empty()) {
			Logger("[FieldEnemyManager] 敵データが空です\n");
			return;
		}

		// 一旦クリア
		enemyDataMap_.clear();
		enemyDataMap_.reserve(json.size() + 4);

		// 1エネミーごとに処理
		for (const auto& [id, jval] : json.items()) {
			FieldEnemyData data;

			// 文字列・float・bool型など基本パラメータ
			data.enemyId = jval.value("enemyId", id);
			if (data.enemyId.empty() || data.enemyId != id) {
				data.enemyId = id;
			}
			data.modelPath = jval.value("modelPath", "");
			data.battleEnemyId = jval.value("battleEnemyId", "");
			data.battleFormation = jval.value("battleFormation", "");
			data.patrolRadius = jval.value("patrolRadius", 1.0f);
			data.patrolSpeed = jval.value("patrolSpeed", 1.0f);
			data.chaseSpeed = jval.value("chaseSpeed", 1.0f);
			data.chaseRange = jval.value("chaseRange", 1.0f);
			data.returnDistance = jval.value("returnDistance", 10.0f);
			data.viewDistance = jval.value("viewDistance", 10.0f);
			data.viewAngle = jval.value("viewAngle", 60.0f);
			data.noiseDetectionRange = jval.value("noiseDetectionRange", 8.0f);
			data.rotationSpeed = jval.value("rotationSpeed", 5.0f);
			data.alertDuration = jval.value("alertDuration", 0.8f);
			data.searchDuration = jval.value("searchDuration", 3.0f);
			data.searchSweepAngle = jval.value("searchSweepAngle", 70.0f);
			data.useCustomColor = jval.value("useCustomColor", false);
			data.useSpotLight = jval.value("useSpotLight", true);
			data.spotLightIntensity = jval.value("spotLightIntensity", 1000.0f);
			data.spotLightDecay = jval.value("spotLightDecay", 0.1f);
			data.spotLightPitch = jval.value("spotLightPitch", 0.0f);

			// スケール/色などJSONオブジェクト型は中のキーごとに
			if (jval.contains("scale")) {
				data.scale.x = jval["scale"].value("x", 1.0f);
				data.scale.y = jval["scale"].value("y", 1.0f);
				data.scale.z = jval["scale"].value("z", 1.0f);
			}
			else {
				data.scale = Vector3(1.f, 1.f, 1.f);
			}

			if (jval.contains("spotLightColor")) {
				data.spotLightColor.x = jval["spotLightColor"].value("r", 1.0f);
				data.spotLightColor.y = jval["spotLightColor"].value("g", 1.0f);
				data.spotLightColor.z = jval["spotLightColor"].value("b", 1.0f);
				data.spotLightColor.w = jval["spotLightColor"].value("a", 1.0f);
			}

			if (jval.contains("spotLightOffset")) {
				data.spotLightOffset.x = jval["spotLightOffset"].value("x", 0.0f);
				data.spotLightOffset.y = jval["spotLightOffset"].value("y", 0.0f);
				data.spotLightOffset.z = jval["spotLightOffset"].value("z", 0.0f);
			}
			else {
				data.spotLightOffset = Vector3(0.0f, 0.0f, 0.0f);
			}

			// vector<string>（複数バトルエネミーIDs）
			if (jval.contains("battleEnemyIds")) {
				data.battleEnemyIds = jval["battleEnemyIds"].get<std::vector<std::string>>();
			}
			else {
				data.battleEnemyIds.clear();
			}

			// battleType: 新フォーマット(enum直接シリアライズ)を優先し、
			// 旧フォーマット(battleTypeStr文字列)のファイルも読めるようフォールバックする。
			if (jval.contains("battleType")) {
				data.battleType = jval.value("battleType", BattleType::Single);
			}
			else {
				const std::string oldStr = jval.value("battleTypeStr", "Single");
				if (oldStr == "Group") data.battleType = BattleType::Group;
				else if (oldStr == "Boss") data.battleType = BattleType::Boss;
				else data.battleType = BattleType::Single;
			}

			// ... 他のパラメータ項目も必要に応じて追加してください ...

			// 最後にMapへ格納
			enemyDataMap_[id] = data;
		}

		Logger("[FieldEnemyManager] 敵データを" + std::to_string(enemyDataMap_.size()) + "件読み込みました\n");
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
			spawnJson["spawnOnLoad"] = data.spawnOnLoad;
			spawnJson["comment"] = data.comment;
			spawnJson["isEditorOnly"] = data.isEditorOnly;

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
			spawnData.comment = spawnJson.value("comment", std::string(""));
			spawnData.isEditorOnly = spawnJson.value("isEditorOnly", false);

			// 先にスポーンマップへ登録 (敵データ未定義でもマーカーは見えるようにするため)。
			// SpawnFieldEnemy は enemyDataMap_ に該当データが無いと早期 return するが、
			// その場合でもエディタ用に位置を可視化したいので spawnDataMap_ には残す。
			spawnData.spawnOnLoad = spawnJson.value("spawnOnLoad", true);
			if (spawnData.id.empty()) {
				spawnData.id = GenerateUniqueSpawnPointId("Spawn");
			}
			else if (spawnDataMap_.contains(spawnData.id)) {
				const std::string oldId = spawnData.id;
				spawnData.id = GenerateUniqueSpawnPointId(oldId);
				Logger("[FieldEnemyManager] 警告: 重複スポーンID '" + oldId + "' を '" + spawnData.id + "' に変更しました\n");
			}
			spawnDataMap_[spawnData.id] = spawnData;
			// spawnOnLoad=false なら自動スポーンせず配置だけ登録する。
			// 後でエディタの「全スポーンを実行」やリスポーン経由で出現させられる。
			if (spawnData.spawnOnLoad) {
				SpawnFieldEnemy(spawnData);
			}
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
	// デバッグ情報表示は FieldEnemyEditorUI に分離済み (神クラス対策)。
	FieldEnemyEditorUI::ShowDebugInfo(*this);
#endif
}

/// <summary>
/// 全敵の描画
/// </summary>
void FieldEnemyManager::Draw() {
#ifdef USE_IMGUI
	if (editorHideEnemies_) return;
#endif
	// 敵は静的メッシュ(.obj)なのでインスタンシングでまとめ描き。
	// IsActive() が Despawn 状態を弾く（従来の Draw 内ガードと同じ）。
	auto* inst = InstancedObject3d::GetInstance();
	inst->Begin(camera_);
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive() && enemy->GetObject3d()) {
			inst->Submit(*enemy->GetObject3d(), enemy->GetWT());
		}
	}
	inst->DrawAll(camera_);
}

void FieldEnemyManager::DrawShadow()
{
#ifdef USE_IMGUI
	if (editorHideEnemies_) return;
#endif
	auto* inst = InstancedObject3d::GetInstance();
	inst->Begin();
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive() && enemy->GetObject3d()) {
			inst->Submit(*enemy->GetObject3d(), enemy->GetWT());
		}
	}
	inst->DrawShadow();
}

/// <summary>
/// 敵コリジョンの描画
/// </summary>
void FieldEnemyManager::DrawCollision() {
#ifdef USE_IMGUI
	if (editorHideEnemies_) return;
#endif
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->DrawCollision();
		}
	}
}

void FieldEnemyManager::DrawLine(Line* line)
{
#ifdef USE_IMGUI
	if (editorHideEnemies_) return;
#endif
	for (auto& enemy : fieldEnemies_) {
		if (enemy && enemy->IsActive()) {
			enemy->DrawLine(line);
		}
	}
}

void FieldEnemyManager::DrawUI()
{
#ifdef USE_IMGUI
	if (editorHideEnemies_) return;
#endif
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
/// スポーンポイントの視覚マーカー (球+縦線) を描画する。
/// FieldScene の DrawLine 経由で呼ばれる (DX12 コマンドリスト発行段階)。
/// </summary>
void FieldEnemyManager::DrawEditorMarkers(Line* /*sharedLine*/) {
#ifdef USE_IMGUI
	if (!markerLineSelected_ || !markerLineActive_ || !markerLineInactive_ || !markerLinePole_) return;

	// 色は各 Line インスタンスに固定で割り当てる (DrawLine は 1 回しか呼ばないため安定)。
	// 色分け: 選択中=赤 / アクティブ=黄 / 無効=灰 / 縦線=白
	markerLineSelected_->SetColor({ 1.0f, 0.15f, 0.15f, 1.0f });
	markerLineActive_  ->SetColor({ 1.0f, 0.85f, 0.15f, 1.0f });
	markerLineInactive_->SetColor({ 0.5f, 0.5f, 0.5f,  0.6f });
	markerLinePole_    ->SetColor({ 1.0f, 1.0f, 1.0f,  0.6f });

	for (const auto& [id, data] : spawnDataMap_) {
		Line* target = nullptr;
		if (id == selectedSpawnId_) target = markerLineSelected_.get();
		else if (data.isActive)     target = markerLineActive_.get();
		else                        target = markerLineInactive_.get();

		target->DrawSphere(data.position, 1.0f, 128);
		markerLinePole_->RegisterLine(data.position, Vector3{ data.position.x, 0.0f, data.position.z });
	}

	// 全頂点を 1 回ずつフラッシュ (GPU バッファ stomp を回避)
	markerLineSelected_->DrawLine();
	markerLineActive_  ->DrawLine();
	markerLineInactive_->DrawLine();
	markerLinePole_    ->DrawLine();
#endif
}

#ifdef USE_IMGUI
/// <summary>
/// 選択中スポーンポイントに ImGuizmo Translate ハンドルを描画する。
/// Editor のギズモコールバック経由で呼ばれる必要がある
/// (ImGuizmo::SetDrawlist がゲームビューのドローリストを掴むため)。
/// </summary>
void FieldEnemyManager::DrawSpawnPointGizmoHandles() {
	if (!camera_ || selectedSpawnId_.empty()) return;

	auto it = spawnDataMap_.find(selectedSpawnId_);
	if (it == spawnDataMap_.end()) return;

	SpawnPointGizmable gz(&it->second, [this]() {
		// 操作終了時: JSON 保存 + 編集中バッファ同期
		SaveEnemySpawnData(FieldEnemyPaths::Spawn);
		auto it2 = spawnDataMap_.find(selectedSpawnId_);
		if (it2 != spawnDataMap_.end()) {
			editorSpawnData_ = it2->second;
		}
	});

	std::vector<IGizmable*> targets = { &gz };
	spawnGizmoCtrl_.Draw(camera_, targets,
		Editor::GetInstance()->GetGameViewPos(),
		Editor::GetInstance()->GetGameViewSize());
}
#endif
