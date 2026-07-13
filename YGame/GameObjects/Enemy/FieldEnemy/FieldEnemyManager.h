#pragma once
#include "FieldEnemy.h"
#include <Systems/Navigation/NavPathfinder.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

#ifdef USE_IMGUI
#include <Debugger/Gizmo/GizmoController.h>
#include <Graphics/Drawer/LineManager/Line.h>
#endif

class Player;
class Camera;
class Line;

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

	// 起動時 (LoadEnemySpawnData) で自動スポーンするか。
	// false にすると配置だけ登録されて敵は生成されず、エディタの
	// 「全スポーンを実行」やリスポーンキュー経由で後から出現させられる。
	bool spawnOnLoad = true;
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

	// フィールド敵の見た目スケール（バトル敵へ引き継ぐ）
	Vector3 encounterScale = Vector3(1.0f, 1.0f, 1.0f);
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
	// デバッグ/データ編集用 ImGui UI (USE_IMGUI 限定)。神クラス化対策として別クラスに分離。
	friend class FieldEnemyEditorUI;
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

	// スポーンポイントの視覚マーカー (球+縦線) を描画する。
	// 通常の Line 描画パスから呼ぶ。ImGuizmo ハンドルは Editor のギズモコールバック側で描画する。
	void DrawEditorMarkers(Line* line);

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

	// spawnDataMap_ に登録されているすべてのスポーンポイントについて、
	// まだ実体が出ていないものを一気にスポーンさせる (バッチ起動用)。
	void SpawnAllPending();

	// 現在フィールドにいる敵をすべてデスポーン (リスポーンキューも空にする)。
	// spawnDataMap_ の登録自体は残るので、再度 SpawnAllPending で出現可能。
	void DespawnAll();
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

	// NavPathfinderを設定（FieldSceneから注入）
	void SetNavPathfinder(NavPathfinder* pf) { navPathfinder_ = pf; }
	NavPathfinder* GetNavPathfinder() const { return navPathfinder_; }

	// エンカウント発生時の処理（FiledScene内で処理）
	void SetEncounterDetailCallback(EncounterDetailCallback callback) {
		encounterDetailCallback_ = callback;
	}

	// 敵 (グループ) 撃破時に発火するコールバック。EventTrigger / OpenGateAction から購読する用途。
	using EnemyDefeatedCallback = std::function<void(const std::string& enemyGroup)>;
	void SetOnEnemyDefeatedCallback(EnemyDefeatedCallback cb) {
		onEnemyDefeatedCallback_ = std::move(cb);
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
	// ShowEnemyEditor/ShowSpawnPointEditor/ShowEnemyDataEditor は FieldEnemyEditorUI に分離済み (神クラス対策)。

	// エディターモードを設定
	void SetEditorMode(bool enabled) { isEditorMode_ = enabled; }

	// エディターモードかどうか取得
	bool IsEditorMode() const { return isEditorMode_; }

	// 撃破リストをクリア
	void ClearDefeatedList();
private:
	///************************* ★エディター内部処理 *************************///
	// データ編集系のヘルパーは FieldEnemyEditorUI に分離済み (神クラス対策)。
	// friend 経由で spawnDataMap_/enemyDataMap_ 等の private メンバへアクセスする。

	// 特定のEnemyIDを持つ全インスタンスにデータを同期する
	void SyncEnemyInstances(const std::string& enemyId);

	// 全フィールド敵を削除
	void RemoveAllFieldEnemies();

	// スポーンポイントIDの空きを探す (LoadEnemySpawnData での重複/空ID救済にも使うため残す)
	std::string GenerateUniqueSpawnPointId(const std::string& prefix) const;

private:
	///************************* メンバ変数 *************************///

	// 外部参照
	Camera* camera_ = nullptr;
	Player* player_ = nullptr;
	EncounterDetailCallback encounterDetailCallback_;
	EnemyDefeatedCallback onEnemyDefeatedCallback_;

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

	// Navigation
	NavPathfinder* navPathfinder_ = nullptr; // FieldSceneから注入

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

#ifdef USE_IMGUI
	// スポーンポイント編集用のギズモコントローラ (ImGuizmo を内包)
	GizmoController spawnGizmoCtrl_;
	// Editor::AddGizmoDrawCallback で取得したコールバックID (Finalize で解除する用)
	int gizmoCallbackId_ = 0;

	// エディタプレビュー用: フィールド敵の描画/コリジョン/UI を全て抑制してスポーン点だけ見せる
	bool editorHideEnemies_ = false;

	// スポーンマーカー用に状態ごとの専用 Line インスタンスを持つ。
	// 単一インスタンスで DrawLine を複数回呼ぶと GPU バッファが上書きされて
	// 最後の 1 回分しか描画されないため、色 (状態) ごとに別バッファに分ける。
	std::unique_ptr<Line> markerLineSelected_;
	std::unique_ptr<Line> markerLineActive_;
	std::unique_ptr<Line> markerLineInactive_;
	std::unique_ptr<Line> markerLinePole_; // 縦線 (地面まで) 用

	// ImGuizmo ハンドル描画 (Editor のゲームビューウィンドウ内コンテキストから呼ばれる)
	void DrawSpawnPointGizmoHandles();
#endif
};
