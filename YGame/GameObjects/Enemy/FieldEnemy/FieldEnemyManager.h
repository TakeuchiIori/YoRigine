#pragma once
#include "FieldEnemy.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

class Player;
class Camera;

///************************* フィールド敵スポーンデータ構造体 *************************///
struct FieldEnemySpawnData {
	std::string id;
	std::string enemyId;  // FieldEnemyData読み込み用
	Vector3 position;
	bool isActive = true;
	std::string spawnCondition;
	bool respawnAfterBattle = true;
	float respawnDelay = 30.0f;

	// ★エディター用追加フィールド
	std::string comment;  // スポーンポイントのメモ
	bool isEditorOnly = false;  // エディター専用フラグ
};

///************************* エンカウント情報構造体 *************************///
struct EncountInfo {
	std::string enemyGroup;
	std::string battleEnemyId;					// 単体バトル用
	std::vector<std::string> battleEnemyIds;	// 複数体バトル用
	Vector3 encounterPosition;
	FieldEnemy* encounteredEnemy;

	// バトル追加情報
	std::string battleFormation;
	BattleType battleType;
};

// エンカウント発生時のコールバック
using EncounterDetailCallback = std::function<void(const EncountInfo& encounterInfo)>;


///************************* フィールド上の敵に関するファイルパス *************************///
namespace FieldEnemyPaths {
	const std::string Spawn = "Resources/Json/FieldEnemies/field_spawn_data.json";
	const std::string EnemyData = "Resources/Json/FieldEnemies/field_enemy_data.json";
}

///************************* フィールド用の敵管理クラス *************************///
class FieldEnemyManager {
public:
	///************************* 基本的な関数 *************************///

	// コンストラクタ
	FieldEnemyManager();

	// デストラクタ
	~FieldEnemyManager();

	// 初期化処理
	void Initialize(Camera* camera);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 影描画処理
	void DrawShadow();

	// 当たり判定の描画
	void DrawCollision();
	void DrawLine(Line* line);

	// UI描画
	void DrawUI();

	// 終了処理
	void Finalize();

	// バトル終了後の処理（リスポーン管理含む）
	void HandleBattleEnd(const std::string& enemyGroup, bool playerWon);


	///************************* フィールド敵管理 *************************///
	// エンカウントのリセット
	void ResetEnCount();
	// すべての敵のアクティブ状態を設定
	void SetAllEnemiesActive(bool isActive);
	// 敵を撃破リストに登録
	void RegisterDefeatedEnemy(const std::string& id);

	// 敵エンカウント時の処理
	void OnEnemyEncounter(FieldEnemy* enemy);
private:
	///************************* 内部処理 *************************///
	// 非アクティブな敵を削除
	void CleanupInactiveEnemies();
	// フィールド敵をスポーン
	void SpawnFieldEnemy(const FieldEnemySpawnData& spawnData);
	// 指定IDの敵を削除
	void RemoveFieldEnemy(const std::string& id);
	// 指定した敵が撃破済みか確認
	bool IsEnemyDefeated(const std::string& id) const;

	// 撃破済み敵をクリア
	void ClearDefeatedEnemies();

	// 敵の状態を更新
	void UpdateEnemyStates();

	// リスポーンタイマーを更新
	void UpdateRespawnTimers();


	///************************* アクセッサ *************************///
public:

	// プレイヤーを設定
	void SetPlayer(Player* player);

	// エンカウント発生時の処理（FiledScene内で処理）
	void SetEncounterDetailCallback(EncounterDetailCallback callback) {
		encounterDetailCallback_ = callback;
	}

	// 敵IDからフィールド敵を取得
	FieldEnemy* GetFieldEnemyById(const std::string& id);

	// 指定範囲内の敵を取得
	std::vector<FieldEnemy*> GetFieldEnemiesInRange(const Vector3& center, float range);

	// アクティブなフィールド敵を取得
	std::vector<FieldEnemy*> GetActiveFieldEnemies();

	// アクティブな敵の数を取得
	size_t GetActiveEnemyCount() const;

	// バトルグループ数を取得（エンカウント単位でカウント）
	size_t GetActiveEncounterGroupCount() const;

	// 指定の敵グループが最後のエンカウントか確認
	bool IsLastEncounterGroup(const std::string& enemyGroup) const;

	// 敵がスポーンされたことがあるかチェック
	bool HasAnyEnemiesBeenSpawned() const { return totalEnemiesSpawned_ > 0; }

	///************************* データ管理 *************************///

	// 敵のスポーン情報をファイルに保存・ロード
	void SaveEnemySpawnData(const std::string& filePath);
	void LoadEnemySpawnData(const std::string& filePath);

	// 敵のデータをファイルに保存・ロード
	void SaveEnemyData(const std::string& filePath);
	void LoadEnemyData(const std::string& filePath);

	///************************* デバッグ *************************///

	// デバッグ情報を表示
	void ShowDebugInfo();

	///************************* エディター機能 *************************///

	// エネミーエディターを表示
	void ShowEnemyEditor();

	// スポーンポイントエディターを表示
	void ShowSpawnPointEditor();

	// 敵データエディターを表示
	void ShowEnemyDataEditor();

	// エディターモードを設定
	void SetEditorMode(bool enabled) { isEditorMode_ = enabled; }

	// エディターモードかどうか取得
	bool IsEditorMode() const { return isEditorMode_; }

	// 撃破リストをクリア
	void ClearDefeatedList();
private:
	///************************* ★エディター内部処理 *************************///

	// 特定のEnemyIDを持つ全インスタンスにデータを同期する
	void SyncEnemyInstances(const std::string& enemyId);

	// 全フィールド敵を削除
	void RemoveAllFieldEnemies();

	// 新しい敵データを作成
	void CreateNewEnemyData();

	// 敵データを編集
	void EditEnemyData(const std::string& enemyId);

	// 敵データを削除
	void DeleteEnemyData(const std::string& enemyId);

	// 新しいスポーンポイントを作成
	void CreateNewSpawnPoint();

	// スポーンポイントを編集
	void EditSpawnPoint(const std::string& spawnId);

	// スポーンポイントを削除
	void DeleteSpawnPoint(const std::string& spawnId);

	// エディター用ギズモを描画
	void DrawEditorGizmos();

private:
	///************************* メンバ変数 *************************///

	// 外部参照
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	EncounterDetailCallback encounterDetailCallback_;

	// フィールド敵管理
	std::vector<std::unique_ptr<FieldEnemy>> fieldEnemies_;
	std::unordered_map<std::string, FieldEnemySpawnData> spawnDataMap_;
	std::unordered_map<std::string, FieldEnemyData> enemyDataMap_;

	// エンカウント管理
	EncountInfo lastEncounterInfo_;
	bool encounterOccurred_ = false;
	float encounterCooldown_ = 0.0f;
	float encounterCooldownDuration_ = 2.0f;

	// リスポーン管理
	struct RespawnInfo {
		FieldEnemySpawnData spawnData;
		float timer;
		bool isWaiting;
	};
	std::vector<RespawnInfo> respawnQueue_;
	std::unordered_set<std::string> defeatedEnemyIds_; // 撃破済み敵ID

	// 設定
	bool isActive_ = true;
	bool showDebugInfo_ = false;

	// スポーン追跡用
	size_t totalEnemiesSpawned_ = 0;

	// エディター関連
	bool isEditorMode_ = false;
	bool showEnemyEditor_ = false;
	bool showSpawnPointEditor_ = false;
	bool showEnemyDataEditor_ = false;
	std::string selectedEnemyId_;
	std::string selectedSpawnId_;
	FieldEnemyData editorEnemyData_;  // 編集中の敵データ
	FieldEnemySpawnData editorSpawnData_;  // 編集中のスポーンデータ
};