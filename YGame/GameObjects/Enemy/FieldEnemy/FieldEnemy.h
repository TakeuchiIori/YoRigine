#pragma once
#include "../Generators/Object3D/BaseObject.h"
#include "../IEnemyState.h"
#include <string>
#include <memory>
#include <vector>

// 状態
enum class FieldEnemyState {
	Patrol,   // 巡回
	Chase,    // 追跡
	Despawn   // 消滅
};
// バトルタイプ
enum class BattleType {
	Single,   // 単体バトル
	Group,    // グループバトル
	Boss      // ボスバトル
};
///************************* フィールド敵データ構造体 *************************///
struct FieldEnemyData {
	std::string enemyId;
	std::string modelPath;

	// 単体バトルの場合
	std::string battleEnemyId;

	// 複数体バトルの場合（優先度が高い）
	std::vector<std::string> battleEnemyIds;

	// バトルフォーメーション名
	std::string battleFormation = "default";


	BattleType battleType = BattleType::Single;

	Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);

	// 巡回パラメータ
	float patrolRadius = 5.0f;
	float patrolSpeed = 2.0f;

	// 追跡パラメータ
	float chaseSpeed = 4.0f;
	float chaseRange = 10.0f;
	float returnDistance = 15.0f;

	// ステルス用パラメータ
	float viewDistance = 15.0f;  // 視界の長さ
	float viewAngle = 60.0f;     // 視野角（中心から左右に30度、計60度）
	float noiseDetectionRange = 8.0f; // 走っている時に見つかる距離

	// ビジュアル設定
	Vector4 modelColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	bool useCustomColor = false;

	// バトル用敵IDリストを取得（単体・複数対応）
	std::vector<std::string> GetBattleEnemyIds() const {
		if (!battleEnemyIds.empty()) {
			return battleEnemyIds;  // 複数体の場合
		}
		return { battleEnemyId };  // 単体の場合
	}

	// バトルタイプ文字列を取得
	std::string GetBattleTypeString() const {
		switch (battleType) {
		case BattleType::Single: return "単体";
		case BattleType::Group: return "グループ";
		case BattleType::Boss: return "ボス";
		default: return "不明";
		}
	}

};

class Player;
class FieldEnemyManager;

///************************* フィールド用の敵クラス *************************///
class FieldEnemy : public BaseObject {
public:
	///************************* 基本的な関数 *************************///

	// コンストラクタ
	FieldEnemy();

	// デストラクタ
	~FieldEnemy() override;

	// 初期化処理（カメラ設定など）
	void Initialize(Camera* camera) override;

	// フィールドデータをもとに初期化
	void InitializeFieldData(const FieldEnemyData& data, const Vector3& spawnPosition);

	// 当たり判定初期化
	void InitCollision() override;

	// JSON初期化（BaseObject継承）
	void InitJson() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// 影描画処理
	void DrawShadow();

	// 当たり判定の可視化描画
	void DrawCollision() override;

	///************************* 当たり判定 *************************///

	// 衝突開始時
	void OnEnterCollision(BaseCollider* self, BaseCollider* other) override;

	// 衝突中
	void OnCollision(BaseCollider* self, BaseCollider* other) override;

	// 衝突終了時
	void OnExitCollision(BaseCollider* self, BaseCollider* other) override;

	// 方向付き衝突（前・後など）
	void OnDirectionCollision(BaseCollider* self, BaseCollider* other, HitDirection dir) override;

	// 方向付き衝突開始時
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);


	///************************* 状態管理 *************************///

	// 状態を変更
	void ChangeState(std::unique_ptr<IEnemyState<FieldEnemy>> newState);

	// 現在の状態を取得
	IEnemyState<FieldEnemy>* GetCurrentState() const { return currentState_.get(); }

	// 論理的状態を取得
	FieldEnemyState GetLogicalState() const { return logicalState_; }

	// 論理的状態を設定
	void SetLogicalState(FieldEnemyState state) { logicalState_ = state; }

	///************************* タイマー制御 *************************///

	// 状態タイマーをリセット
	void ResetStateTimer() { stateTimer_ = 0.0f; }

	// 状態タイマーを加算
	void AddStateTimer(float dt) { stateTimer_ += dt; }

	// 状態タイマーを取得
	float GetStateTimer() const { return stateTimer_; }

	///************************* アクセッサ *************************///

	// アクセッサ・ユーティリティセクションに追加
	void ApplyUpdatedData(const FieldEnemyData& data);

	// 敵データを取得
	const FieldEnemyData& GetEnemyData() const { return enemyData_; }

	// バトル用敵IDを取得（単体の場合）
	const std::string& GetBattleEnemyId() const { return enemyData_.battleEnemyId; }

	// ★バトル用敵IDリストを取得（複数体対応）
	std::vector<std::string> GetBattleEnemyIds() const { return enemyData_.GetBattleEnemyIds(); }

	// ★バトルタイプを取得
	BattleType GetBattleType() const { return enemyData_.battleType; }

	// 敵グループ名を取得
	std::string GetEnemyGroupName() const { return enemyData_.enemyId; }

	// 現在位置を取得
	Vector3 GetPosition() const { return wt_.translate_; }

	// 現在の座標を取得
	Vector3 GetTranslate() const { return wt_.translate_; }

	// 位置を設定
	void SetTranslate(const Vector3& pos) { wt_.translate_ = pos; }

	// 位置を加算
	void AddTranslate(const Vector3& delta) { wt_.translate_ += delta; }

	// スポーン位置を取得
	Vector3 GetSpawnPosition() const { return spawnPosition_; }

	// 巡回ターゲット位置を取得
	Vector3 GetPatrolTarget() const { return patrolTarget_; }

	// 巡回ターゲット位置を設定
	void SetPatrolTarget(const Vector3& target) { patrolTarget_ = target; }

	// Y軸回転を設定
	void SetRotationY(float y) { wt_.rotate_.y = y; }

	// Y軸回転を取得
	float GetRotationY() const { return wt_.rotate_.y; }

	// プレイヤーを設定
	void SetPlayer(Player* player) { player_ = player; }

	// プレイヤーを取得
	Player* GetPlayer() const { return player_; }

	// プレイヤーの座標を取得
	Vector3 GetPlayerPosition() const;

	// プレイヤーが存在するか
	bool HasPlayer() const { return player_ != nullptr; }

	// 管理クラスを設定
	void SetFieldEnemyManager(FieldEnemyManager* manager) { fieldEnemyManager_ = manager; }

	// エンカウント済みか確認
	bool HasTriggeredEncounter() const { return hasTriggeredEncounter_; }

	// エンカウント状態をリセット（旧メソッド - 互換性のため残す）
	void ResetEncounterTrigger() { ResetEncounterState(); }

	// エンカウント状態を完全リセット（推奨）
	void ResetEncounterState();

	// エンカウント可能か確認
	bool CanTriggerEncounter() const { return encounterCooldown_ <= 0.0f && !hasTriggeredEncounter_; }

	// エンカウントクールダウンを更新
	void UpdateEncounterCooldown(float dt);

	// エンカウントクールダウンの残り時間を取得
	float GetEncounterCooldown() const { return encounterCooldown_; }

	// 敵がアクティブか確認
	bool IsActive() const { return logicalState_ != FieldEnemyState::Despawn; }

	// 敵をデスポーン状態にする
	void Despawn() { logicalState_ = FieldEnemyState::Despawn; }

	// ID
	std::string GetSpawnId() const { return spawnId_; }
	void SetSpawnId(const std::string& id) { spawnId_ = id; }

	// 事前に食らったダメージ
	float GetTakeDamage() const { return takeDamage_; }

private:
	///************************* 内部処理 *************************///
	// エンカウント発生をトリガー
	void TriggerEncounter();

	// ダメージを食らう処理
	void TakeDamage(const float damage);

private:
	///************************* メンバ変数 *************************///

	// 現在の状態クラス
	std::unique_ptr<IEnemyState<FieldEnemy>> currentState_;

	// 状態タイマー
	float stateTimer_ = 0.0f;

	// 現在の論理状態
	FieldEnemyState logicalState_ = FieldEnemyState::Patrol;

	// 敵データ
	FieldEnemyData enemyData_;
	std::string spawnId_;

	// 外部参照
	Player* player_ = nullptr;
	FieldEnemyManager* fieldEnemyManager_ = nullptr;

	// 位置情報
	Vector3 spawnPosition_;
	Vector3 patrolTarget_;

	// エンカウント管理
	bool hasTriggeredEncounter_ = false;
	float encounterCooldown_ = 0.0f;
	float encounterCooldownDuration_ = 1.0f;

	float takeDamage_ = 0.0f;
};
