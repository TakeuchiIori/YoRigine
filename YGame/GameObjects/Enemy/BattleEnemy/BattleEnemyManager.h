#pragma once
#include "BattleEnemy.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

class Player;
class Camera;

///************************* 戦闘フォーメーションデータ *************************///
struct BattleFormationData {
	std::string formationName;
	std::vector<Vector3> positions;
	std::string description;
};

///************************* 敵エンカウントデータ *************************///
struct EnemyEncounterData {
	std::string encounterName;
	std::vector<std::string> enemyIds;
	std::vector<Vector3> formations;
	std::string battleBackground = "default";
	std::string bgm = "battle_default";
	bool isBossEncounter = false;
	int minLevel = 1;
	int maxLevel = 5;
	float encounterRate = 1.0f;
	bool isOnlyOnce = false;
	std::string requiredFlag;
};

///************************* 戦闘結果列挙 *************************///
enum class BattleResult {
	None,
	Victory,
	Defeat,
	Escape,
	InProgress
};

///************************* 戦闘統計データ *************************///
struct BattleStats {
	int totalExpGained = 0;
	int totalGaldGained = 0;
	int enemiesDefeated = 0;
	float battleDuration = 0.0f;
	std::string enemyID = "";
	std::vector<std::string> droppedItems;
};

// 戦闘終了時のコールバック
using BattleEndCallback = std::function<void(BattleResult result, const BattleStats& stats)>;

///************************* 戦闘用の敵管理クラス *************************///
class BattleEnemyManager {
public:
	///************************* 基本的な関数 *************************///

	// コンストラクタ
	BattleEnemyManager();

	// デストラクタ
	~BattleEnemyManager();

	// 初期化処理
	void Initialize(Camera* camera);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// UI描画処理
	void DrawUI();

	// 影描画処理
	void DrawShadow();

	// 当たり判定描画
	void DrawCollision();

	// 終了処理
	void Finalize();

	///************************* 戦闘管理 *************************///

	// 戦闘開始（エンカウント名指定）
	void StartBattle(const std::string& encounterName);

	// 戦闘開始（エンカウントデータ指定）
	void StartBattle(const EnemyEncounterData& encounterData);

	// 戦闘終了
	void EndBattle(BattleResult result);

	// 強制戦闘終了
	void ForceBattleEnd();

	// 敵を生成
	void SpawnBattleEnemy(const std::string& enemyId, const Vector3& position);

	// 敵グループを生成
	void SpawnEnemyGroup(const std::vector<std::string>& enemyIds, const std::vector<Vector3>& positions);

	// すべての敵を削除
	void RemoveAllBattleEnemies();

	// 戦闘中かどうか取得
	bool IsBattleActive() const { return isBattleActive_; }

	// 全ての敵が撃破されたか確認
	bool AreAllEnemiesDefeated() const;

	// プレイヤーが撃破されたか確認
	bool IsPlayerDefeated() const;

	// 戦闘の一時停止
	void PauseBattle(bool isPaused) { isBattlePaused_ = isPaused; }

	///************************* 敵管理 *************************///

	// 全敵にターゲットを設定
	void SetAllEnemiesTarget(Player* player);

	// 全敵をスタン状態にする
	void StunAllEnemies(float duration);

	// 全敵にダメージを与える
	void DamageAllEnemies(int damage);

	// アクティブな敵一覧を取得
	std::vector<BattleEnemy*> GetActiveBattleEnemies();

	// 指定範囲内の敵を取得
	std::vector<BattleEnemy*> GetEnemiesInRange(const Vector3& center, float range);

	// 最も近い敵を取得
	BattleEnemy* GetNearestEnemy(const Vector3& position);

	// IDで敵を取得
	BattleEnemy* GetEnemyById(const std::string& id);

	// アクティブな敵の数を取得
	size_t GetActiveEnemyCount() const;

	// デフォルトフォーメーションを読み込み
	void LoadDefaultFormations();

	///************************* フォーメーション管理 *************************///

	// フォーメーションデータを読み込み
	void LoadFormations(const std::string& filePath);

	// 使用するフォーメーションを設定
	void SetFormation(const std::string& formationName);

	// 指定名のフォーメーションを取得
	BattleFormationData GetFormation(const std::string& formationName) const;

	// 敵数に応じたフォーメーション座標を取得
	std::vector<Vector3> GetFormationPositions(size_t enemyCount) const;

	///************************* データ管理 *************************///

	// 敵データの保存・読み込み
	bool SaveEnemyData(const std::string& filePath) const;
	bool LoadEnemyData(const std::string& filePath);

	// 指定エンカウントデータを取得
	EnemyEncounterData GetEncounterData(const std::string& encounterName) const;

	// 現在の戦闘統計を取得
	const BattleStats& GetBattleStats() const { return battleStats_; }

	// 戦闘統計をリセット
	void ResetBattleStats();

	///************************* アクセッサ *************************///

	// プレイヤーを設定
	void SetPlayer(Player* player) { player_ = player; }

	// 戦闘終了コールバックを設定
	void SetBattleEndCallback(BattleEndCallback callback) { battleEndCallback_ = callback; }

	// 最終バトルモードを設定
	void SetFinalBattleMode(bool isFinal) { isFinalBattle_ = isFinal; }

	// 最終バトルをクリアしたか取得
	bool IsFinalBattleCleared() const { return isFinalBattleCleared_; }

	// 最終バトルクリアフラグをリセット
	void ResetFinalBattleClearFlag() { isFinalBattleCleared_ = false; }

	// 戦闘結果を取得
	BattleResult GetBattleResult() const { return battleResult_; }

	// 戦闘経過時間を取得
	float GetBattleTimer() const { return battleTimer_; }

	// 現在のエンカウント名を取得
	std::string GetCurrentEncounterName() const { return currentEncounterName_; }

	///************************* デバッグ *************************///

	// デバッグ情報を表示
	void ShowDebugInfo();

	// デバッグ用敵スポーン
	void DebugSpawnEnemy(const Vector3& position, const std::string& enemyId);

private:
	///************************* 内部処理 *************************///

	// 戦闘状態を更新
	void UpdateBattleState();

	// 戦闘タイマーを更新
	void UpdateBattleTimer();

	// 戦闘終了条件を確認
	void CheckBattleEndConditions();

	// 敵撃破時の処理
	void OnEnemyDefeated(BattleEnemy* enemy);

	// 戦闘報酬を計算
	void CalculateBattleRewards();

	// 撃破済み敵のクリーンアップ
	void CleanupDefeatedEnemies();

	// デフォルトフォーメーション座標を取得
	Vector3 GetDefaultFormationPosition(size_t index, size_t totalCount) const;

private:
	///************************* メンバ変数 *************************///

	// 外部参照
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	BattleEndCallback battleEndCallback_;

	// 敵管理
	std::vector<std::unique_ptr<BattleEnemy>> battleEnemies_;
	std::unordered_map<std::string, BattleEnemyData> enemyDataMap_;
	const std::string enemyDataFilePath_ = "Resources/Json/BattleEnemies/enemy_data.json";

	// 戦闘状態
	bool isBattleActive_ = false;
	bool isBattlePaused_ = false;
	BattleResult battleResult_ = BattleResult::None;
	float battleTimer_ = 0.0f;

	// 最終バトル管理
	bool isFinalBattle_ = false;
	bool isFinalBattleCleared_ = false;  //  最終バトルクリアフラグ
	bool isWaitingForClearTransition_ = false;
	float finalBattleSlowTimer_ = 0.0f;

	// エンカウント管理
	std::string currentEncounterName_;
	EnemyEncounterData currentEncounter_;
	std::unordered_map<std::string, EnemyEncounterData> encounterDataMap_;

	// フォーメーション管理
	std::unordered_map<std::string, BattleFormationData> formationMap_;
	std::string currentFormation_ = "default";

	// 戦闘統計
	BattleStats battleStats_;

	// 設定・AI更新
	bool showDebugInfo_ = false;
	float aiUpdateInterval_ = 0.1f;
	float aiUpdateTimer_ = 0.0f;
};