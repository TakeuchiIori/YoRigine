#include "BattleEnemyManager.h"
#include "Player/Player.h"

// C++
#include <algorithm>
#include <random>

// Engine
#include "Systems/GameTime/GameTime.h"
#include <Loaders/Json/JsonManager.h>
#include <Debugger/Logger.h>

// Math
#include "Vector3.h"
#include "MathFunc.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Collision/AreaCollision/Base/AreaManager.h>
#include <SceneSystems/SceneManager.h>
#include <Drawer/InstancedObject3d.h>
#include "Object3D/BaseObjectManager.h"

namespace {
	// BaseObjectManager 登録名の一意連番（プロセス内で単調増加）
	uint32_t g_battleEnemyRegSeq = 0;
}

/// <summary>
/// コンストラクタ
/// </summary>
BattleEnemyManager::BattleEnemyManager() = default;

/// <summary>
/// デストラクタ
/// </summary>
BattleEnemyManager::~BattleEnemyManager() = default;

/// <summary>
/// 敵マネージャーの初期化処理
/// </summary>
/// <param name="camera">使用するカメラのポインタ</param>
void BattleEnemyManager::Initialize(Camera* camera) {
	camera_ = camera;
	battleEnemies_.clear();   // 解除は BattleEnemy デストラクタが行う
	enemyDataMap_.clear();
	encounterDataMap_.clear();
	formationMap_.clear();

	// 戦闘状態リセット
	isBattleActive_ = false;
	isBattlePaused_ = false;
	battleResult_ = BattleResult::None;
	battleTimer_ = 0.0f;
	currentEncounterName_.clear();

	// 統計リセット
	ResetBattleStats();
	// デフォルトフォーメーション設定
	LoadDefaultFormations();
	// 敵データの読み込み
	LoadEnemyData(enemyDataFilePath_);
}

/// <summary>
/// 戦闘中の全体更新処理
/// </summary>
void BattleEnemyManager::Update() {

	if (!isBattleActive_ || isBattlePaused_) return;

	float deltaTime = YoRigine::GameTime::GetDeltaTime();

	// 戦闘タイマー更新
	UpdateBattleTimer();

	// 各敵の更新
	UpdateBattleState();

	// 一定間隔でAIを更新
	aiUpdateTimer_ += deltaTime;
	if (aiUpdateTimer_ >= aiUpdateInterval_) {
		aiUpdateTimer_ = 0.0f;
	}

	// 倒された敵を先に削除（OnEnemyDefeatedが先に実行）
	CleanupDefeatedEnemies();

	// 終了条件チェック（OnEnemyDefeated後に実行）
	CheckBattleEndConditions();
}
/// <summary>
/// 各敵の状態を更新
/// </summary>
void BattleEnemyManager::UpdateBattleState() {
	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			enemy->Update();
		}
	}
}

/// <summary>
/// 戦闘タイマーを更新
/// </summary>
void BattleEnemyManager::UpdateBattleTimer() {
	if (isBattleActive_) {
		battleTimer_ += YoRigine::GameTime::GetDeltaTime();
		battleStats_.battleDuration = battleTimer_;
	}
}

/// <summary>
/// 戦闘終了条件を確認し、該当すれば終了処理を行う
/// </summary>
void BattleEnemyManager::CheckBattleEndConditions() {
	if (battleResult_ != BattleResult::None) return;

	// 最終バトルでクリアシーン遷移待ち中は終了処理をスキップ
	if (isWaitingForClearTransition_) {
		return;
	}

	// 全敵撃破チェック
	if (AreAllEnemiesDefeated()) {
		// 最終バトルの場合はOnEnemyDefeatedで既に処理済み
		if (isFinalBattle_) {
			Logger("[BattleEnemyManager] Final battle - victory handled in OnEnemyDefeated\n");
			return;
		}

		Logger("[BattleEnemyManager] 全敵撃破！勝利判定\n");
		EndBattle(BattleResult::Victory);
		return;
	}

	// プレイヤー敗北チェック
	if (IsPlayerDefeated()) {
		Logger("[BattleEnemyManager] プレイヤー敗北判定\n");
		EndBattle(BattleResult::Defeat);
		return;
	}

	// 他の終了条件があればここに追加
}

/// <summary>
/// 倒された敵を削除し、統計を更新
/// </summary>
void BattleEnemyManager::CleanupDefeatedEnemies() {
	battleEnemies_.erase(
		std::remove_if(battleEnemies_.begin(), battleEnemies_.end(),
			[this](const std::unique_ptr<BattleEnemy>& enemy) {
				if (enemy && !enemy->IsAlive()) {
					OnEnemyDefeated(enemy.get());
					return true;   // erase → BattleEnemy デストラクタで登録解除
				}
				return false;
			}),
		battleEnemies_.end()
	);
}

/// <summary>
/// 敵撃破時の処理（統計更新）
/// </summary>
/// <param name="enemy">撃破された敵</param>
void BattleEnemyManager::OnEnemyDefeated(BattleEnemy* enemy) {
	if (!enemy) return;

	const auto& enemyData = enemy->GetEnemyData();
	Logger("[BattleEnemyManager] 敵撃破: " + enemyData.enemyId + "\n");

	// 統計更新
	battleStats_.enemiesDefeated++;
	if (enemyDefeatedCallback_) {
		enemyDefeatedCallback_(*enemy);
	}

	// 最終バトル時：残りの生存敵をカウント
	if (isFinalBattle_) {
		size_t aliveCount = 0;
		for (const auto& e : battleEnemies_) {
			if (e && e->IsAlive()) {
				aliveCount++;
			}
		}

		char debugBuffer[256];
		sprintf_s(debugBuffer, "[BattleEnemyManager] Final Battle - Remaining alive enemies: %zu\n", aliveCount);
		Logger(debugBuffer);

		// 生存敵が0になった = 全敵撃破！
		if (aliveCount == 0) {
			Logger("[BattleEnemyManager] ★★★ FINAL ENEMY DEFEATED! ★★★\n");

			// スローモーション演出開始
			// 最終バトルクリアフラグを立てる
			isFinalBattleCleared_ = true;

			// n秒後にクリアシーンへ遷移
			finalBattleSlowTimer_ = 0.0f;
			isWaitingForClearTransition_ = true;

			Logger("[BattleEnemyManager] Slow motion started, transitioning to Clear in 1 sec\n");
		}
	}
}

/// <summary>
/// 戦闘を開始（エンカウント名を指定）
/// </summary>
/// <param name="encounterName">エンカウント名</param>
void BattleEnemyManager::StartBattle(const std::string& encounterName) {
	auto it = encounterDataMap_.find(encounterName);
	if (it != encounterDataMap_.end()) {
		StartBattle(it->second);
	} else {
		Logger("[BattleEnemyManager] エラー: エンカウント名が見つかりません: " + encounterName + "\n");
	}
}

/// <summary>
/// 戦闘を開始（エンカウンターデータを指定）
/// </summary>
/// <param name="encounterData">エンカウンターデータ構造体</param>
void BattleEnemyManager::StartBattle(const EnemyEncounterData& encounterData) {
	// 既に戦闘中ならリセット
	if (isBattleActive_) {
		Logger("[BattleEnemyManager] 前回の戦闘が継続中、敵をクリア\n");
		RemoveAllBattleEnemies();
		EndBattle(BattleResult::None);
	}

	// 念のため、戦闘開始前に敵をクリア
	if (!battleEnemies_.empty()) {
		Logger("[BattleEnemyManager] 新規戦闘前に既存敵をクリア\n");
		RemoveAllBattleEnemies();
	}

	// データ設定
	currentEncounter_ = encounterData;
	currentEncounterName_ = encounterData.encounterName;

	// 状態初期化
	isBattleActive_ = true;
	isBattlePaused_ = false;
	battleResult_ = BattleResult::None;
	battleTimer_ = 0.0f;

	// 統計リセット
	ResetBattleStats();

	Logger("[BattleEnemyManager] 戦闘開始: " + encounterData.encounterName +
		" 敵数: " + std::to_string(encounterData.enemyIds.size()) + "\n");

	// 敵を生成（フィールド敵から引き継いだスケールを全敵に適用）
	SpawnEnemyGroup(encounterData.enemyIds, encounterData.formations, encounterData.enemyScale);

	Logger("[BattleEnemyManager] " + std::to_string(battleEnemies_.size()) + "体の敵を生成\n");

	// 全敵にプレイヤーをターゲットとして設定
	SetAllEnemiesTarget(player_);
}

/// <summary>
/// 戦闘を終了し、結果を記録・後処理を実行する
/// </summary>
/// <param name="result">戦闘結果（勝利・敗北・逃走など）</param>
void BattleEnemyManager::EndBattle(BattleResult result) {
	if (!isBattleActive_) return;

	// 最終バトル勝利時はスロー演出処理に任せる
	if (isFinalBattle_ && result == BattleResult::Victory) {
		Logger("[BattleEnemyManager] Final battle victory - handled by slow motion transition\n");
		return;  // コールバックを呼ばずに終了
	}

	battleResult_ = result;
	isBattleActive_ = false;

	const char* resultStr = (result == BattleResult::Victory) ? "勝利" :
		(result == BattleResult::Defeat) ? "敗北" :
		(result == BattleResult::Escape) ? "逃走" : "不明";

	Logger("[BattleEnemyManager] 戦闘終了: " + std::string(resultStr) + "\n");

	// 最終統計計算
	CalculateBattleRewards();

	// コールバック実行
	if (battleEndCallback_) {
		battleEndCallback_(result, battleStats_);
	}

	// 戦闘後の後処理
	if (result == BattleResult::Victory || result == BattleResult::Defeat) {
		// 少し待ってから敵を削除
		RemoveAllBattleEnemies();
	}
}

/// <summary>
/// 強制的に戦闘を終了する（結果なし）
/// </summary>
void BattleEnemyManager::ForceBattleEnd() {
	if (isBattleActive_) {
		Logger("[BattleEnemyManager] 戦闘を強制終了\n");
		EndBattle(BattleResult::None);
		RemoveAllBattleEnemies();
	}
}


/// <summary>
/// 指定した敵IDと位置から敵を生成
/// </summary>
/// <param name="enemyId">敵ID</param>
/// <param name="position">生成位置</param>
void BattleEnemyManager::SpawnBattleEnemy(const std::string& enemyId, const Vector3& position, const Vector3& scale) {
	if (!camera_) {
		ThrowError("[BattleEnemyManager] エラー: カメラが設定されていません\n");
		return;
	}

	// enemyDataMap_ に敵IDが存在するかチェックする
	auto it = enemyDataMap_.find(enemyId);
	if (it == enemyDataMap_.end()) {
		// データが事前ロードされていない場合はエラーとし、生成を中断する
		ThrowError(("[BattleEnemyManager] エラー: 敵データID \"" + enemyId + "\" がキャッシュにありません。LoadEnemyData()を事前に実行してください。\n").c_str());
		return;
	}

	// キャッシュからデータを取得する
	const BattleEnemyData& enemyData = it->second;
	Logger(("[BattleEnemyManager] キャッシュから敵データ取得: " + enemyId + "\n").c_str());

	// 敵オブジェクトの生成と初期化
	auto newEnemy = std::make_unique<BattleEnemy>();
	newEnemy->Initialize(camera_);
	newEnemy->SetPlayer(player_);

	// キャッシュから取得したデータと位置情報で初期化（フィールド敵の見た目スケールを引き継ぐ）
	newEnemy->InitializeBattleData(enemyData, position, scale);

	// エリアマネージャーに登録
	AreaManager::GetInstance()->RegisterObject(&newEnemy->GetWT(), ("Enemy_" + enemyId).c_str());
	battleEnemies_.push_back(std::move(newEnemy));

	// BaseObjectManager へ一意名で登録（インスペクタ一覧・名前検索用。描画は当マネージャが担当）
	BaseObjectManager::GetInstance()->Register(
		battleEnemies_.back().get(),
		"BattleEnemy_" + std::to_string(g_battleEnemyRegSeq++));

	Logger(("[BattleEnemyManager] 敵を生成: " + enemyId + " 位置: (" +
		std::to_string(position.x) + ", " + std::to_string(position.y) + ", " +
		std::to_string(position.z) + ") 合計: " + std::to_string(battleEnemies_.size()) + "体\n").c_str());
}
/// <summary>
/// 敵グループを生成
/// </summary>
/// <param name="enemyIds">生成する敵IDのリスト</param>
/// <param name="positions">配置位置リスト</param>
void BattleEnemyManager::SpawnEnemyGroup(const std::vector<std::string>& enemyIds, const std::vector<Vector3>& positions, const Vector3& scale) {
	size_t enemyCount = enemyIds.size();

	Logger("[BattleEnemyManager] 敵グループ生成開始: " + std::to_string(enemyCount) + "体\n");

	for (size_t i = 0; i < enemyCount; ++i) {
		Vector3 spawnPos;

		if (i < positions.size()) {
			spawnPos = positions[i];
		} else {
			spawnPos = GetDefaultFormationPosition(i, enemyCount);
		}

		SpawnBattleEnemy(enemyIds[i], spawnPos, scale);
	}
}

/// <summary>
/// 全ての戦闘中の敵を削除
/// </summary>
void BattleEnemyManager::RemoveAllBattleEnemies() {
	size_t count = battleEnemies_.size();
	auto* areaManager = AreaManager::GetInstance();

	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			// エリアの登録解除（BaseObjectManager の解除は BattleEnemy デストラクタ）
			areaManager->UnregisterObject(&enemy->GetWT());
		}
	}

	if (count > 0) {
		Logger("[BattleEnemyManager] 全ての敵を削除: " + std::to_string(count) + "体\n");
	}
	battleEnemies_.clear();
	// 戦闘状態もリセット
	isBattleActive_ = false;
}

/// <summary>
/// 全ての敵が撃破されているか確認
/// </summary>
/// <returns>全敵が倒されていれば true</returns>
bool BattleEnemyManager::AreAllEnemiesDefeated() const {
	return std::all_of(battleEnemies_.begin(), battleEnemies_.end(),
		[](const std::unique_ptr<BattleEnemy>& enemy) {
			return !enemy || !enemy->IsAlive();
		});
}

/// <summary>
/// プレイヤーが敗北状態か確認
/// </summary>
/// <returns>敗北状態なら true</returns>
bool BattleEnemyManager::IsPlayerDefeated() const {
	if (player_) {
		// TODO: 実際のHPチェック実装予定
		// return player_->GetCurrentHP() <= 0;
		return false;
	}
	return false;
}

/// <summary>
/// 全ての敵にプレイヤーをターゲットとして設定
/// </summary>
/// <param name="player">ターゲットに設定するプレイヤー</param>
void BattleEnemyManager::SetAllEnemiesTarget(Player* player) {
	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->SetPlayer(player);
		}
	}
}

/// <summary>
/// 全ての敵をスタン状態にする
/// </summary>
/// <param name="duration">スタンの継続時間</param>
void BattleEnemyManager::StunAllEnemies([[maybe_unused]] float duration) {
	Logger("[BattleEnemyManager] 全敵スタン: " + std::to_string(duration) + "秒\n");
}

/// <summary>
/// 全ての敵にダメージを与える
/// </summary>
/// <param name="damage">与えるダメージ量</param>
void BattleEnemyManager::DamageAllEnemies(int damage) {
	Logger("[BattleEnemyManager] 全敵にダメージ: " + std::to_string(damage) + "\n");

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			enemy->TakeDamage(damage);
		}
	}
}

/// <summary>
/// アクティブな敵を取得
/// </summary>
/// <returns>生存中の敵リスト</returns>
std::vector<BattleEnemy*> BattleEnemyManager::GetActiveBattleEnemies() {
	std::vector<BattleEnemy*> result;

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			result.push_back(enemy.get());
		}
	}
	return result;
}

/// <summary>
/// 指定範囲内の敵を取得
/// </summary>
/// <param name="center">中心座標</param>
/// <param name="range">範囲距離</param>
/// <returns>範囲内にいる敵リスト</returns>
std::vector<BattleEnemy*> BattleEnemyManager::GetEnemiesInRange(const Vector3& center, float range) {
	std::vector<BattleEnemy*> result;

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			float distance = Length(enemy->GetTranslate() - center);
			if (distance <= range) {
				result.push_back(enemy.get());
			}
		}
	}
	return result;
}

/// <summary>
/// 最も近い敵を取得
/// </summary>
/// <param name="position">基準位置</param>
/// <returns>最も近い敵へのポインタ</returns>
BattleEnemy* BattleEnemyManager::GetNearestEnemy(const Vector3& position) {
	BattleEnemy* nearest = nullptr;
	float minDistance = FLT_MAX;

	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->IsAlive()) {
			float distance = Length(enemy->GetTranslate() - position);
			if (distance < minDistance) {
				minDistance = distance;
				nearest = enemy.get();
			}
		}
	}
	return nearest;
}

/// <summary>
/// 敵IDから敵を取得
/// </summary>
/// <param name="id">敵ID</param>
/// <returns>該当する敵へのポインタ（見つからない場合nullptr）</returns>
BattleEnemy* BattleEnemyManager::GetEnemyById(const std::string& id) {
	for (auto& enemy : battleEnemies_) {
		if (enemy && enemy->GetEnemyData().enemyId == id) {
			return enemy.get();
		}
	}
	return nullptr;
}

/// <summary>
/// 生存している敵の数を取得
/// </summary>
/// <returns>アクティブな敵数</returns>
size_t BattleEnemyManager::GetActiveEnemyCount() const {
	return std::count_if(battleEnemies_.begin(), battleEnemies_.end(),
		[](const std::unique_ptr<BattleEnemy>& enemy) {
			return enemy && enemy->IsAlive();
		});
}

/// <summary>
/// デフォルトフォーメーションを設定
/// </summary>
void BattleEnemyManager::LoadDefaultFormations() {
	BattleFormationData single;
	single.formationName = "single";
	single.description = "単体敵用の中央配置";
	single.positions = { Vector3(0.0f, 0.0f, 0.0f) };
	formationMap_["single"] = single;

	BattleFormationData dual;
	dual.formationName = "dual";
	dual.description = "2体の敵を左右に配置";
	dual.positions = {
		Vector3(0.0f, 0.0f, -10.0f),
		Vector3(0.0f, 0.0f, 10.0f)
	};
	formationMap_["dual"] = dual;

	BattleFormationData triple;
	triple.formationName = "triple";
	triple.description = "3体の敵を横一列に配置";
	triple.positions = {
		Vector3(0.0f, 0.0f, -10.0f),
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, 0.0f, 10.0f)
	};
	formationMap_["triple"] = triple;

	BattleFormationData quad;
	quad.formationName = "quad";
	quad.description = "4体の敵を2x2で広めに配置";
	quad.positions = {
		// 前列
		Vector3(0.0f, 0.0f, -7.5f),
		Vector3(0.0f, 0.0f, 7.5f),
		// 後列
		Vector3(0.0f, 0.0f, -12.0f),
		Vector3(0.0f, 0.0f, 12.0f)
	};
	formationMap_["quad"] = quad;

	Logger("[BattleEnemyManager] デフォルトフォーメーション読み込み完了\n");
}

/// <summary>
/// フォーメーションデータを外部ファイルから読み込む
/// </summary>
/// <param name="filePath">ファイルパス</param>
void BattleEnemyManager::LoadFormations([[maybe_unused]] const std::string& filePath) {
	Logger("[BattleEnemyManager] フォーメーションデータ読み込み: " + filePath + "\n");
}

/// <summary>
/// 現在使用するフォーメーションを設定
/// </summary>
/// <param name="formationName">フォーメーション名</param>
void BattleEnemyManager::SetFormation(const std::string& formationName) {
	currentFormation_ = formationName;
	Logger("[BattleEnemyManager] フォーメーション設定: " + formationName + "\n");
}

/// <summary>
/// 指定フォーメーション名に対応するデータを取得
/// </summary>
/// <param name="formationName">フォーメーション名</param>
/// <returns>フォーメーションデータ</returns>
BattleFormationData BattleEnemyManager::GetFormation(const std::string& formationName) const {
	auto it = formationMap_.find(formationName);
	if (it != formationMap_.end()) {
		return it->second;
	}
	Logger("[BattleEnemyManager] 警告: フォーメーション名が見つかりません: " + formationName + "\n");
	return BattleFormationData();
}

/// <summary>
/// 敵数に応じたフォーメーション位置を取得
/// </summary>
/// <param name="enemyCount">敵の数</param>
/// <returns>配置位置リスト</returns>
std::vector<Vector3> BattleEnemyManager::GetFormationPositions(size_t enemyCount) const {
	std::string formationName;

	switch (enemyCount) {
	case 1: formationName = "single"; break;
	case 2: formationName = "dual"; break;
	case 3: formationName = "triple"; break;
	case 4: formationName = "quad"; break;
	default: formationName = "single"; break;
	}

	auto it = formationMap_.find(formationName);
	if (it != formationMap_.end()) {
		return it->second.positions;
	}

	std::vector<Vector3> positions;
	for (size_t i = 0; i < enemyCount; ++i) {
		positions.push_back(GetDefaultFormationPosition(i, enemyCount));
	}
	return positions;
}
/// <summary>
/// デフォルトフォーメーション位置を取得
/// </summary>
/// <param name="index">インデックス</param>
/// <param name="totalCount">総数</param>
/// <returns>配置座標</returns>
Vector3 BattleEnemyManager::GetDefaultFormationPosition(size_t index, size_t totalCount) const {
	float spacing = 2.5f;
	float startX = -(spacing * (totalCount - 1)) / 2.0f;
	float x = startX + (spacing * index);
	return Vector3(x, 0.0f, 5.0f);
}

/// <summary>
/// 指定名のエンカウンターデータを取得
/// </summary>
/// <param name="encounterName">エンカウント名</param>
/// <returns>該当するデータ（なければ空データ）</returns>
EnemyEncounterData BattleEnemyManager::GetEncounterData(const std::string& encounterName) const {
	auto it = encounterDataMap_.find(encounterName);
	if (it != encounterDataMap_.end()) {
		return it->second;
	}
	Logger("[BattleEnemyManager] 警告: エンカウンターデータが見つかりません: " + encounterName + "\n");
	return EnemyEncounterData();
}

/// <summary>
/// 戦闘統計をリセット
/// </summary>
void BattleEnemyManager::ResetBattleStats() {
	battleStats_ = BattleStats();
}

/// <summary>
/// 戦闘報酬を計算
/// </summary>
void BattleEnemyManager::CalculateBattleRewards() {
	Logger("[BattleEnemyManager] 戦闘報酬計算\n");
	Logger("[BattleEnemyManager] 撃破数: " + std::to_string(battleStats_.enemiesDefeated) +
		" 経験値: " + std::to_string(battleStats_.totalExpGained) +
		" ゴールド: " + std::to_string(battleStats_.totalGaldGained) + "\n");
}

bool BattleEnemyManager::SaveEnemyData(const std::string& filePath) const {
	json j = json::object();
	json enemyArray = json::array();

	// enemyDataMap_ のデータをJSON配列に変換
	for (const auto& pair : enemyDataMap_) {
		const BattleEnemyData& data = pair.second;

		json enemyJson = {
			{"enemyId", data.enemyId},
			{"modelPath", data.modelPath},
			{"level", data.level},
			{"hp", data.hp}, // ベースHPを保存
			{"attack", data.attack},
			{"defense", data.defense},
			{"moveSpeed", data.moveSpeed},
			{"approachStateRange", data.approachStateRange},
			{"attackStateRange", data.attackStateRange},
			{"attackPatterns", data.attackPatterns}
		};

		// 攻撃パラメータを構造化して保存
		json ap = json::object();

		// 突進攻撃 (Rush)
		ap["rush"] = {
			{"chargeTime", data.attackParams.rush.chargeTime},
			{"rushTime", data.attackParams.rush.rushTime},
			{"speedMultiplier", data.attackParams.rush.speedMultiplier},
			{"cooldownTime", data.attackParams.rush.cooldownTime}
		};

		// チャージ攻撃 (chargeRush)
		ap["chargeRush"] = {
			{"chargeTime", data.attackParams.chargeRush.chargeTime},
			{"rushTime", data.attackParams.chargeRush.rushTime},
			{"speedMultiplier", data.attackParams.chargeRush.speedMultiplier},
			{"cooldownTime", data.attackParams.chargeRush.cooldownTime}
		};

		// 回転攻撃 (Spin)
		ap["spin"] = {
			{"chargeTime", data.attackParams.spin.chargeTime},
			{"spinTime", data.attackParams.spin.spinTime},
			{"rotationCount", data.attackParams.spin.rotationCount},
			{"moveSpeedMultiplier", data.attackParams.spin.moveSpeedMultiplier},
			{"cooldownTime", data.attackParams.spin.cooldownTime}
		};

		// ジャンプ攻撃 (jump)
		ap["jump"] = {
			{"chargeTime", data.attackParams.jump.chargeTime},
			{"jumpTime", data.attackParams.jump.jumpTime},
			{"jumpHeight", data.attackParams.jump.jumpHeight},
			{"crouchDepth", data.attackParams.jump.crouchDepth},
			{"cooldownTime", data.attackParams.jump.cooldownTime}
		};

		// コンボ攻撃 (Combo)
		ap["combo"] = {
			{"phaseDuration", data.attackParams.combo.phaseDuration},
			{"subChargeTime", data.attackParams.combo.subChargeTime},
			{"subRushTime", data.attackParams.combo.subRushTime},
			{"rushSpeedMultiplier", data.attackParams.combo.rushSpeedMultiplier},
			{"cooldownTime", data.attackParams.combo.cooldownTime}
		};

		// カウンター攻撃 (Counter) — 連続被弾→気合溜め→突進反撃
		ap["counter"] = {
			{"enabled", data.attackParams.counter.enabled},
			{"triggerHitCount", data.attackParams.counter.triggerHitCount},
			{"hitCountResetTime", data.attackParams.counter.hitCountResetTime},
			{"recoveryDuration", data.attackParams.counter.recoveryDuration},
			{"startupTime", data.attackParams.counter.startupTime},
			{"anticipationTime", data.attackParams.counter.anticipationTime},
			{"anticipationDistance", data.attackParams.counter.anticipationDistance},
			{"chargeTime", data.attackParams.counter.chargeTime},
			{"rushTime", data.attackParams.counter.rushTime},
			{"rushSpeedMultiplier", data.attackParams.counter.rushSpeedMultiplier},
			{"rushHomingStrength", data.attackParams.counter.rushHomingStrength},
			{"cooldownTime", data.attackParams.counter.cooldownTime}
		};

		// メインのJSONに攻撃パラメータを追加
		enemyJson["attackParams"] = ap;
		enemyArray.push_back(enemyJson);
	}

	j["battleEnemies"] = enemyArray;

	// ファイルに書き出し
	std::ofstream ofs(filePath);
	if (!ofs.is_open()) {
		ThrowError(("敵データ保存用のファイルを開けませんでした: " + filePath + "\n").c_str());
		return false;
	}

	try {
		// インデント4で整形して保存
		ofs << std::setw(4) << j;
		ofs.close();
		Logger((std::to_string(enemyDataMap_.size()) + "件の敵データを正常に保存しました。\n").c_str());
		return true;
	}
	catch (const std::exception& e) {
		Logger(("敵データの保存中にエラーが発生しました: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}
/// <summary>
/// 全ての敵のベースデータをJSONファイルから読み込み、キャッシュする
/// </summary>
bool BattleEnemyManager::LoadEnemyData(const std::string& filePath) {
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		ThrowError(("敵データファイルを開けませんでした: " + filePath + "\n").c_str());
		return false;
	}

	try {
		json j = json::parse(ifs);

		if (!j.contains("battleEnemies") || !j["battleEnemies"].is_array()) {
			ThrowError(("敵データファイルの形式が無効です: 'battleEnemies'配列が見つかりません。\n"));
			return false;
		}

		// 既存のデータをクリア
		enemyDataMap_.clear();

		for (const auto& enemyJson : j["battleEnemies"]) {
			if (!enemyJson.contains("enemyId")) {
				ThrowError("敵データエントリに'enemyId'がありません。スキップします。\n");
				continue;
			}

			BattleEnemyData data{};
			data.enemyId = enemyJson["enemyId"].get<std::string>();

			// JSONの値を取得。存在しない場合はデフォルト値を適用
			data.modelPath = enemyJson.value("modelPath", "default_enemy.obj");
			data.level = enemyJson.value("level", 1);
			data.hp = enemyJson.value("hp", 100);
			data.attack = enemyJson.value("attack", 15);
			data.defense = enemyJson.value("defense", 10);
			data.moveSpeed = enemyJson.value("moveSpeed", 5.0f);
			data.approachStateRange = enemyJson.value("approachStateRange", 15.0f);
			data.attackStateRange = enemyJson.value("attackStateRange", 10.0f);

			// 攻撃パターンを読み込み
			data.attackPatterns.clear();
			if (enemyJson.contains("attackPatterns") && enemyJson["attackPatterns"].is_array()) {
				for (const auto& pattern : enemyJson["attackPatterns"]) {
					data.attackPatterns.push_back(pattern.get<std::string>());
				}
			}

			if (enemyJson.contains("attackParams")) {
				const auto& ap = enemyJson["attackParams"];

				// 突進攻撃 (Rush)
				if (ap.contains("rush")) {
					const auto& c = ap["rush"];
					auto& target = data.attackParams.rush;
					target.anticipationTime = c.value("anticipationTime", 0.5f);
					target.anticipationDistance = c.value("anticipationDistance", 1.5f);

					target.chargeTime = c.value("chargeTime", 1.0f);
					target.rushTime = c.value("rushTime", 0.5f);
					target.speedMultiplier = c.value("speedMultiplier", 7.0f);
					target.cooldownTime = c.value("cooldownTime", 1.2f);
				}

				// チャージ攻撃 (chargeRush)
				if (ap.contains("chargeRush")) {
					const auto& c = ap["chargeRush"];
					auto& target = data.attackParams.chargeRush;
					target.anticipationTime = c.value("anticipationTime", 0.8f);
					target.stompIntensity = c.value("stompIntensity", 0.4f);
					target.anticipationColorPulseSpeed = c.value("anticipationColorPulseSpeed", 8.0f);
					
					target.chargeTime = c.value("chargeTime", 1.5f);
					target.rushTime = c.value("rushTime", 0.5f);
					target.speedMultiplier = c.value("speedMultiplier", 12.0f);
					target.cooldownTime = c.value("cooldownTime", 1.2f);
				}

				// 回転攻撃 (spin)
				if (ap.contains("spin")) {
					const auto& l = ap["spin"];
					auto& target = data.attackParams.spin;
					target.anticipationTime = l.value("anticipationTime", 0.5f);
					target.twistAngle = l.value("twistAngle", 1.57f);
					target.anticipationColorIntensity = l.value("anticipationColorIntensity", 0.7f);

					target.chargeTime = l.value("chargeTime", 0.3f);
					target.spinTime = l.value("spinTime", 1.0f);
					target.rotationCount = l.value("rotationCount", 2.0f);
					target.moveSpeedMultiplier = l.value("moveSpeedMultiplier", 2.0f);
					target.cooldownTime = l.value("cooldownTime", 0.5f);
				}

				// ジャンプ攻撃 (jump)
				if (ap.contains("jump")) {
					const auto& l = ap["jump"];
					auto& target = data.attackParams.jump;
					target.anticipationTime = l.value("anticipationTime", 0.7f);
					target.anticipationCrouchDepth = l.value("anticipationCrouchDepth", 0.8f);
					target.anticipationColorPulseSpeed = l.value("anticipationColorPulseSpeed", 6.0f);

					target.chargeTime = l.value("chargeTime", 0.5f);
					target.jumpTime = l.value("jumpTime", 0.7f);
					target.jumpHeight = l.value("jumpHeight", 4.0f);
					target.crouchDepth = l.value("crouchDepth", 0.3f);
					target.cooldownTime = l.value("cooldownTime", 0.6f);
				}

				// コンボ攻撃 (Combo)
				if (ap.contains("combo")) {
					const auto& cb = ap["combo"];
					auto& target = data.attackParams.combo;
					target.phaseDuration = cb.value("phaseDuration", 0.8f);
					target.subChargeTime = cb.value("subChargeTime", 0.4f);
					target.subRushTime = cb.value("subRushTime", 0.2f);
					target.rushSpeedMultiplier = cb.value("rushSpeedMultiplier", 8.0f);
					target.cooldownTime = cb.value("cooldownTime", 0.8f);
				}

				// カウンター攻撃 (Counter)
				if (ap.contains("counter")) {
					const auto& ct = ap["counter"];
					auto& target = data.attackParams.counter;
					target.enabled              = ct.value("enabled", true);
					target.triggerHitCount      = ct.value("triggerHitCount", 4);
					target.hitCountResetTime    = ct.value("hitCountResetTime", 2.5f);
					target.recoveryDuration     = ct.value("recoveryDuration", 1.5f);
					target.startupTime          = ct.value("startupTime", 0.2f);
					target.anticipationTime     = ct.value("anticipationTime", 0.5f);
					target.anticipationDistance = ct.value("anticipationDistance", 5.0f);
					target.chargeTime           = ct.value("chargeTime", 0.25f);
					target.rushTime             = ct.value("rushTime", 0.55f);
					target.rushSpeedMultiplier  = ct.value("rushSpeedMultiplier", 15.0f);
					target.rushHomingStrength   = ct.value("rushHomingStrength", 1.5f);
					target.cooldownTime         = ct.value("cooldownTime", 0.8f);
				}
			}

			// マップにデータを格納
			enemyDataMap_[data.enemyId] = data;
		}
		Logger((std::to_string(enemyDataMap_.size()) + "件の敵データを正常に読み込み、キャッシュしました。\n").c_str());
		return true;

	}
	catch (const json::parse_error& e) {
		ThrowError(("敵データファイル内でJSON解析エラーが発生しました: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
	catch (const std::exception& e) {
		ThrowError(("敵データの読み込み中にエラーが発生しました: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}

/// <summary>
/// デバッグ用の敵生成
/// </summary>
/// <param name="position">生成位置</param>
/// <param name="enemyId">敵ID</param>
void BattleEnemyManager::DebugSpawnEnemy(const Vector3& position, const std::string& enemyId) {
	Logger("[BattleEnemyManager] デバッグ: 敵生成 " + enemyId + "\n");
	SpawnBattleEnemy(enemyId, position);
}

/// <summary>
/// 敵の描画処理
/// </summary>
void BattleEnemyManager::Draw() {
	if (!isBattleActive_) return;

	// 敵は静的メッシュ(.obj)なのでインスタンシングでまとめ描き。
	// ただし死亡時ディゾルブ中の敵はインスタンシング PSO が非対応なので個別描画にフォールバック。
	auto* inst = InstancedObject3d::GetInstance();
	inst->Begin(camera_);
	for (auto& enemy : battleEnemies_) {
		if (!enemy || !enemy->GetObject3d()) continue;
		if (enemy->GetObject3d()->IsDissolveEnabled()) {
			enemy->Draw();   // ディゾルブは個別描画（Draw 内で ApplyDeathFade も走る）
		} else {
			enemy->ApplyDeathFade();   // 旧 Draw と同じ発火条件で色を更新（Submit が読む）
			inst->Submit(*enemy->GetObject3d(), enemy->GetWT());
		}
	}
	inst->DrawAll(camera_);
}

void BattleEnemyManager::DrawUI()
{
	if (!isBattleActive_) return;
	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->DrawUI();
		}
	}
}

void BattleEnemyManager::DrawShadow()
{
	if (!isBattleActive_) return;

	// 影パスもインスタンシング（影は WVP 不使用なのでカメラ不要）。
	// ディゾルブ中の敵は色パスと揃えて個別影描画にフォールバック。
	auto* inst = InstancedObject3d::GetInstance();
	inst->Begin();
	for (auto& enemy : battleEnemies_) {
		if (!enemy || !enemy->GetObject3d()) continue;
		if (enemy->GetObject3d()->IsDissolveEnabled()) {
			enemy->DrawShadow();
		} else {
			inst->Submit(*enemy->GetObject3d(), enemy->GetWT());
		}
	}
	inst->DrawShadow();
}

/// <summary>
/// 当たり判定の描画（デバッグ用）
/// </summary>
void BattleEnemyManager::DrawCollision() {
	if (!isBattleActive_) return;

	for (auto& enemy : battleEnemies_) {
		if (enemy) {
			enemy->DrawCollision();
		}
	}
}

/// <summary>
/// 終了処理
/// </summary>
void BattleEnemyManager::Finalize() {
	Logger("[BattleEnemyManager] 終了処理開始\n");

	RemoveAllBattleEnemies();
	encounterDataMap_.clear();
	formationMap_.clear();
	enemyDataMap_.clear();

	camera_ = nullptr;
	player_ = nullptr;
	battleEndCallback_ = nullptr;

	Logger("[BattleEnemyManager] 終了処理完了\n");
}


/// <summary>
/// デバッグ情報をImGui上に表示
/// </summary>
void BattleEnemyManager::ShowDebugInfo() {
#ifdef USE_IMGUI
	if (ImGui::Button("敵データ読み込み")) {
		LoadEnemyData(enemyDataFilePath_);
	}
	ImGui::Text("戦闘中: %s", isBattleActive_ ? "はい" : "いいえ");
	ImGui::Text("一時停止: %s", isBattlePaused_ ? "はい" : "いいえ");
	ImGui::Text("アクティブな敵: %zu", GetActiveEnemyCount());
	ImGui::Text("戦闘時間: %.1f秒", battleTimer_);
	ImGui::Text("現在のエンカウント: %s", currentEncounterName_.c_str());

	const char* resultStrings[] = { "なし", "勝利", "敗北", "逃走", "進行中" };
	ImGui::Text("戦闘結果: %s", resultStrings[static_cast<int>(battleResult_)]);

	ImGui::Separator();

	ImGui::Text("=== 戦闘統計 ===");
	ImGui::Text("撃破数: %d", battleStats_.enemiesDefeated);
	ImGui::Text("戦闘時間: %.1f秒", battleStats_.battleDuration);

	ImGui::Separator();

	ImGui::SameLine();
	if (ImGui::Button("戦闘終了")) {
		ForceBattleEnd();
	}

	if (isBattleActive_) {
		if (ImGui::Button("一時停止/再開")) {
			PauseBattle(!isBattlePaused_);
		}

		ImGui::SameLine();
		if (ImGui::Button("勝利")) {
			EndBattle(BattleResult::Victory);
		}
		ImGui::SameLine();
		if (ImGui::Button("敗北")) {
			EndBattle(BattleResult::Defeat);
		}
	}

	ImGui::Separator();

	if (ImGui::Button("全敵スタン(2秒)")) {
		StunAllEnemies(2.0f);
	}
	ImGui::SameLine();
	if (ImGui::Button("全敵ダメージ(50)")) {
		DamageAllEnemies(50);
	}

	ImGui::Separator();

	static char enemyIdBuffer[256] = "goblin";
	static float spawnPos[3] = { 0.0f, 0.0f, 5.0f };

	ImGui::InputText("敵ID", enemyIdBuffer, sizeof(enemyIdBuffer));
	ImGui::InputFloat3("生成位置", spawnPos);

	if (ImGui::Button("デバッグ生成")) {
		Vector3 position(spawnPos[0], spawnPos[1], spawnPos[2]);
		DebugSpawnEnemy(position, enemyIdBuffer);
	}

	ImGui::Separator();

	// ★敵ベースデータ編集（攻撃パターン編集機能追加）★
	if (ImGui::TreeNode("敵ベースデータ編集 （これを調整するとその敵全部に反映）")) {

		// 使用可能な攻撃パターンのリスト
		static const char* availablePatterns[] = {
			"rush", "jump", "spin", "chargeRush", "sidestep", "combo"
		};
		static const int patternCount = 6;

		for (auto& pair : enemyDataMap_) {
			BattleEnemyData& data = pair.second;

			if (ImGui::TreeNode(data.enemyId.c_str())) {

				ImGui::DragInt("HP", &data.hp, 1, 1, 9999);
				ImGui::DragInt("攻撃力", &data.attack, 1, 1, 999);
				ImGui::DragInt("防御力", &data.defense, 1, 1, 999);
				ImGui::DragFloat("移動速度", &data.moveSpeed, 0.1f, 0.1f, 50.0f);
				ImGui::DragFloat("追跡開始距離", &data.approachStateRange, 0.5f, 1.0f, 100.0f);
				ImGui::DragFloat("攻撃開始距離", &data.attackStateRange, 0.5f, 1.0f, 50.0f);

				// --- 攻撃詳細パラメータ ---
				if (ImGui::CollapsingHeader("攻撃詳細設定 (各Stateの数値)")) {

					// --- Rush ---
					if (ImGui::TreeNode("Rush (基本突進)")) {
						auto& r = data.attackParams.rush;
						ImGui::DragFloat("予備動作", &r.anticipationTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 後退する距離", &r.anticipationDistance, 0.05f, 0.0f, 5.0f, "%.2秒");
						ImGui::DragFloat("溜め時間", &r.chargeTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("突進時間", &r.rushTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("速度倍率", &r.speedMultiplier, 0.1f, 0.0f, 20.0f, "x%.1f");
						ImGui::DragFloat("後隙", &r.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- chargeRush ---
					if (ImGui::TreeNode("chargeRush (強力突進)")) {
						auto& c = data.attackParams.chargeRush;
						ImGui::DragFloat("溜め(追尾)時間", &c.chargeTime, 0.05f, 0.0f, 10.0f, "%.2f秒");
						ImGui::DragFloat("突進時間", &c.rushTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("速度倍率", &c.speedMultiplier, 0.1f, 0.0f, 30.0f, "x%.1f");
						ImGui::DragFloat("後隙", &c.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- Spin ---
					if (ImGui::TreeNode("Spin (回転攻撃)")) {
						auto& s = data.attackParams.spin;
						ImGui::DragFloat("予備動作", &s.chargeTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat("回転時間", &s.spinTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("回転数", &s.rotationCount, 0.1f, 0.0f, 10.0f, "%.1f回");
						ImGui::DragFloat("移動倍率", &s.moveSpeedMultiplier, 0.1f, 0.0f, 10.0f, "x%.1f");
						ImGui::DragFloat("後隙", &s.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- jump ---
					if (ImGui::TreeNode("jump (ジャンプ攻撃)")) {
						auto& l = data.attackParams.jump;
						ImGui::DragFloat("踏み込み時間", &l.chargeTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat("滞空時間", &l.jumpTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::DragFloat("ジャンプ高度", &l.jumpHeight, 0.1f, 0.0f, 20.0f, "%.1fm");
						ImGui::DragFloat("しゃがみ深さ", &l.crouchDepth, 0.05f, 0.0f, 2.0f, "%.2fm");
						ImGui::DragFloat("後隙", &l.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- Combo ---
					if (ImGui::TreeNode("Combo (3連撃)")) {
						auto& cb = data.attackParams.combo;
						ImGui::DragFloat("1段の時間", &cb.phaseDuration, 0.05f, 0.1f, 5.0f, "%.2f秒");
						ImGui::DragFloat("段内溜め", &cb.subChargeTime, 0.05f, 0.0f, 2.0f, "%.2f秒");
						ImGui::DragFloat("段内突進", &cb.subRushTime, 0.05f, 0.0f, 2.0f, "%.2f秒");
						ImGui::DragFloat("加速倍率", &cb.rushSpeedMultiplier, 0.1f, 0.0f, 20.0f, "x%.1f");
						ImGui::DragFloat("全体後隙", &cb.cooldownTime, 0.05f, 0.0f, 5.0f, "%.2f秒");
						ImGui::TreePop();
					}

					// --- Counter ---
					if (ImGui::TreeNode("Counter (反撃)")) {
						auto& ct = data.attackParams.counter;
						ImGui::Checkbox("反撃有効", &ct.enabled);
						ImGui::Separator();
						ImGui::TextDisabled("トリガー条件");
						ImGui::DragInt(" 連続被弾しきい値", &ct.triggerHitCount, 1, 1, 20);
						ImGui::DragFloat(" カウントリセット秒数", &ct.hitCountResetTime, 0.1f, 0.1f, 10.0f, "%.1f秒");
						ImGui::Separator();
						ImGui::TextDisabled("Recovery（気合溜め・無敵）");
						ImGui::DragFloat(" 回復時間", &ct.recoveryDuration, 0.05f, 0.1f, 5.0f, "%.2f秒");
						ImGui::Separator();
						ImGui::TextDisabled("CounterAttack 内部フェーズ");
						ImGui::DragFloat(" 起動時間", &ct.startupTime, 0.05f, 0.0f, 2.0f, "%.2f秒");
						ImGui::DragFloat(" 後退時間", &ct.anticipationTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 後退距離", &ct.anticipationDistance, 0.1f, 0.0f, 20.0f, "%.1fm");
						ImGui::DragFloat(" 溜め時間", &ct.chargeTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 突進時間", &ct.rushTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::DragFloat(" 突進速度倍率", &ct.rushSpeedMultiplier, 0.5f, 0.0f, 50.0f, "x%.1f");
						ImGui::DragFloat(" 突進ホーミング強度", &ct.rushHomingStrength, 0.1f, 0.0f, 10.0f, "%.1f");
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("大きいほどプレイヤーを追尾する。1.5前後で読み避け可能");
						ImGui::DragFloat(" クールダウン", &ct.cooldownTime, 0.05f, 0.0f, 3.0f, "%.2f秒");
						ImGui::TreePop();
					}
				}

				ImGui::Separator();

				// 攻撃パターン編集セクション
				if (ImGui::TreeNode("攻撃パターン設定")) {

					ImGui::Text("現在の攻撃パターン:");

					// 現在の攻撃パターン一覧表示と削除
					for (size_t i = 0; i < data.attackPatterns.size(); ++i) {
						ImGui::PushID(static_cast<int>(i));

						ImGui::BulletText("%s", data.attackPatterns[i].c_str());
						ImGui::SameLine();

						if (ImGui::SmallButton("削除")) {
							data.attackPatterns.erase(data.attackPatterns.begin() + i);
							--i; // インデックス調整
							ImGui::PopID();
							continue;
						}

						// 順序変更ボタン
						if (i > 0) {
							ImGui::SameLine();
							if (ImGui::SmallButton("↑")) {
								std::swap(data.attackPatterns[i], data.attackPatterns[i - 1]);
							}
						}
						if (i < data.attackPatterns.size() - 1) {
							ImGui::SameLine();
							if (ImGui::SmallButton("↓")) {
								std::swap(data.attackPatterns[i], data.attackPatterns[i + 1]);
							}
						}

						ImGui::PopID();
					}

					if (data.attackPatterns.empty()) {
						ImGui::TextColored(ImVec4(1, 0, 0, 1), "警告: 攻撃パターンが設定されていません！");
					}

					ImGui::Separator();

					// 新しい攻撃パターンを追加
					static int selectedPatternIndex = 0;
					ImGui::Combo("追加する攻撃", &selectedPatternIndex, availablePatterns, patternCount);

					ImGui::SameLine();
					if (ImGui::Button("追加")) {
						std::string newPattern = availablePatterns[selectedPatternIndex];

						// 重複チェック
						bool alreadyExists = false;
						for (const auto& pattern : data.attackPatterns) {
							if (pattern == newPattern) {
								alreadyExists = true;
								break;
							}
						}

						if (!alreadyExists) {
							data.attackPatterns.push_back(newPattern);
							Logger(("[BattleEnemyManager] 攻撃パターン追加: " + data.enemyId + " -> " + newPattern + "\n").c_str());
						} else {
							Logger(("[BattleEnemyManager] 警告: " + newPattern + " は既に存在します\n").c_str());
						}
					}

					ImGui::SameLine();
					if (ImGui::Button("全てクリア")) {
						data.attackPatterns.clear();
						Logger(("[BattleEnemyManager] 攻撃パターンをクリア: " + data.enemyId + "\n").c_str());
					}

					ImGui::Separator();

					// プリセットボタン
					ImGui::Text("クイック設定:");

					if (ImGui::Button("基本型 (rush)")) {
						data.attackPatterns = { "rush" };
					}
					ImGui::SameLine();
					if (ImGui::Button("アグレッシブ (rush, chargeRush, combo)")) {
						data.attackPatterns = { "rush", "chargeRush", "combo" };
					}

					if (ImGui::Button("トリッキー (sidestep, spin)")) {
						data.attackPatterns = { "sidestep", "spin" };
					}
					ImGui::SameLine();
					if (ImGui::Button("全種類")) {
						data.attackPatterns = { "rush", "jump", "spin", "chargeRush", "combo" };
					}

					ImGui::TreePop();
				}

				ImGui::Separator();

				// 現在の設定を生成中の敵に適用
				if (ImGui::Button("この設定を生成中の同種敵に適用")) {
					int appliedCount = 0;
					for (auto& enemy : battleEnemies_) {
						if (enemy && enemy->GetEnemyData().enemyId == data.enemyId) {
							// 生成中の敵のデータを更新
							BattleEnemyData& enemyData = enemy->GetEnemyData();
							enemyData.attack = data.attack;
							enemyData.defense = data.defense;
							enemyData.moveSpeed = data.moveSpeed;
							enemyData.approachStateRange = data.approachStateRange;
							enemyData.attackStateRange = data.attackStateRange;
							enemyData.attackPatterns = data.attackPatterns;
							enemyData.attackParams = data.attackParams;
							appliedCount++;
						}
					}
					Logger(("[BattleEnemyManager] " + std::to_string(appliedCount) + "体の敵に設定を適用しました\n").c_str());
				}

				ImGui::TreePop();
			}
		}

		ImGui::Separator();

		// データ保存ボタン
		if (ImGui::Button("全ての変更をJSONに保存")) {
			if (SaveEnemyData(enemyDataFilePath_)) {
				Logger("[BattleEnemyManager] 敵ベースデータをJSONファイルに保存しました。\n");
			} else {
				ThrowError("[BattleEnemyManager] 敵ベースデータの保存に失敗しました。\n");
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("JSONから再読み込み")) {
			LoadEnemyData(enemyDataFilePath_);
		}

		ImGui::TreePop();
	}

	ImGui::Separator();

	// アクティブな敵の編集
	if (ImGui::TreeNode("アクティブな敵")) {
		for (size_t i = 0; i < battleEnemies_.size(); ++i) {
			auto& enemy = battleEnemies_[i];
			if (enemy) {
				std::string label = "敵 " + std::to_string(i) + " (" + enemy->GetEnemyData().enemyId + ")";
				if (ImGui::TreeNode(label.c_str())) {
					// 基本情報
					ImGui::Text("HP: %d / %d", enemy->GetCurrentHP(), enemy->GetMaxHP());
					ImGui::Text("生存: %s", enemy->IsAlive() ? "はい" : "いいえ");

					Vector3 pos = enemy->GetTranslate();
					ImGui::Text("位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

					// 個体データ（テスト用に編集可能）
					BattleEnemyData& enemyData = enemy->GetEnemyData();
					ImGui::Text("敵ID: %s", enemyData.enemyId.c_str());
					ImGui::Text("モデル: %s", enemyData.modelPath.c_str());
					ImGui::Text("攻撃力: %d (Base:%d)", enemyData.attack, enemyData.attack);
					ImGui::Text("防御力: %d (Base:%d)", enemyData.defense, enemyData.defense);

					// 個体のパラメータ調整（リアルタイムテスト用）
					ImGui::DragFloat("移動速度 (Current)", &enemyData.moveSpeed, 0.1f, 0.0f, 20.0f);
					ImGui::DragFloat("攻撃状態に入る距離 (Current)", &enemyData.attackStateRange, 0.1f, 0.0f, 100.0f);
					ImGui::DragFloat("追跡状態に入る距離 (Current)", &enemyData.approachStateRange, 0.1f, 0.0f, 100.0f);

					// 攻撃パターン表示
					ImGui::Text("攻撃パターン:");
					for (const auto& pattern : enemyData.attackPatterns) {
						ImGui::BulletText("%s", pattern.c_str());
					}

					// 敵操作ボタン
					if (ImGui::Button("ダメージ(25)")) {
						enemy->TakeDamage(25);
					}
					ImGui::SameLine();
					if (ImGui::Button("回復(30)")) {
						enemy->Heal(30);
					}
					ImGui::SameLine();
					if (ImGui::Button("即死")) {
						enemy->TakeDamage(enemy->GetCurrentHP());
					}

					ImGui::TreePop();
				}
			}
		}
		ImGui::TreePop();
	}

	// フォーメーション
	if (ImGui::TreeNode("フォーメーション")) {
		for (const auto& pair : formationMap_) {
			const auto& formation = pair.second;
			if (ImGui::TreeNode(formation.formationName.c_str())) {
				ImGui::Text("説明: %s", formation.description.c_str());
				ImGui::Text("位置数: %zu", formation.positions.size());
				for (size_t i = 0; i < formation.positions.size(); ++i) {
					const auto& pos = formation.positions[i];
					ImGui::Text("  %zu: (%.1f, %.1f, %.1f)", i, pos.x, pos.y, pos.z);
				}
				if (ImGui::Button("フォーメーション設定")) {
					SetFormation(formation.formationName);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif
}
