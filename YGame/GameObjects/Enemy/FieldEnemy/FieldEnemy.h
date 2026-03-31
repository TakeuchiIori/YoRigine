#pragma once
#include "../Generators/Object3D/BaseObject.h"
#include "../IEnemyState.h"
#include <string>
#include <memory>
#include <vector>
#include "Graphics/Drawer/LineManager/Line.h"

// 状態
enum class FieldEnemyState {
	Patrol,   // 巡回
	Alert,    // 発見リアクション（!）
	Chase,    // 追跡
	Search,   // 索敵（見失い後）
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
	float viewDistance = 15.0f;      // 視界の長さ
	float viewAngle = 60.0f;         // 視野角（中心から左右に30度、計60度）
	float noiseDetectionRange = 8.0f; // 走っている時に見つかる距離

	// 回転・リアクション パラメータ
	float rotationSpeed = 5.0f;     // 補間回転速度（rad/s）。大きいほど素早く向く
	float alertDuration = 0.8f;     // 発見リアクション持続時間（秒）
	float searchDuration = 3.0f;    // 索敵持続時間（秒）
	float searchSweepAngle = 70.0f; // 索敵時の左右スウィープ角度（度）

	// スポットライト(視界)設定
	bool useSpotLight = true;
	Vector4 spotLightColor = Vector4(1.0f, 0.2f, 0.2f, 1.0f);
	float spotLightIntensity = 5.0f;
	float spotLightDecay = 2.0f;
	Vector3 spotLightOffset = Vector3(0.0f, 1.0f, 0.0f); // 目の高さなどのオフセット
	float spotLightPitch = -0.2f;                        // ライトの下向き加減(方向ベクトルのY成分)


	// ビジュアル設定
	Vector4 modelColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	bool useCustomColor = false;

	// バトル用敵IDリストを取得（単体・複数対応）
	std::vector<std::string> GetBattleEnemyIds() const {
		if (!battleEnemyIds.empty()) {
			return battleEnemyIds;
		}
		return { battleEnemyId };
	}

	// バトルタイプ文字列を取得
	std::string GetBattleTypeString() const {
		switch (battleType) {
		case BattleType::Single: return "単体";
		case BattleType::Group: return "グループ";
		case BattleType::Boss:  return "ボス";
		default:                return "不明";
		}
	}
};

class Player;
class FieldEnemyManager;

///************************* フィールド用の敵クラス *************************///
class FieldEnemy : public BaseObject {
public:
	///************************* 基本的な関数 *************************///

	FieldEnemy();
	~FieldEnemy() override;

	void Initialize(Camera* camera) override;
	void InitializeFieldData(const FieldEnemyData& data, const Vector3& spawnPosition);
	void InitCollision() override;
	void InitJson() override;
	void Update() override;
	void Draw() override;
	void DrawShadow();
	void DrawCollision() override;
	void DrawLine(Line* line);

	///************************* 当たり判定 *************************///

	void OnEnterCollision(BaseCollider* self, BaseCollider* other) override;
	void OnCollision(BaseCollider* self, BaseCollider* other) override;
	void OnExitCollision(BaseCollider* self, BaseCollider* other) override;
	void OnDirectionCollision(BaseCollider* self, BaseCollider* other, HitDirection dir) override;
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);

	///************************* 状態管理 *************************///

	void ChangeState(std::unique_ptr<IEnemyState<FieldEnemy>> newState);
	IEnemyState<FieldEnemy>* GetCurrentState() const { return currentState_.get(); }
	FieldEnemyState GetLogicalState() const { return logicalState_; }
	void SetLogicalState(FieldEnemyState state) { logicalState_ = state; }

	///************************* タイマー制御 *************************///

	void ResetStateTimer() { stateTimer_ = 0.0f; }
	void AddStateTimer(float dt) { stateTimer_ += dt; }
	float GetStateTimer() const { return stateTimer_; }

	///************************* 回転ユーティリティ *************************///

	/// <summary>
	/// 目標角度に向かって補間回転する（毎フレーム呼ぶ）
	/// </summary>
	/// <param name="targetAngle">目標Y回転角（ラジアン）</param>
	/// <param name="speed">回転速度（rad/s）</param>
	/// <param name="dt">デルタタイム</param>
	void RotateTowards(float targetAngle, float speed, float dt);

	/// <summary>
	/// プレイヤー方向へ補間回転する
	/// </summary>
	void RotateTowardsPlayer(float speed, float dt);

	/// <summary>
	/// 移動方向へ補間回転する
	/// </summary>
	void RotateTowardsDirection(const Vector3& direction, float speed, float dt);

	///************************* アクセッサ *************************///

	void ApplyUpdatedData(const FieldEnemyData& data);
	const FieldEnemyData& GetEnemyData() const { return enemyData_; }

	const std::string& GetBattleEnemyId() const { return enemyData_.battleEnemyId; }
	std::vector<std::string> GetBattleEnemyIds() const { return enemyData_.GetBattleEnemyIds(); }
	BattleType GetBattleType() const { return enemyData_.battleType; }
	std::string GetEnemyGroupName() const { return enemyData_.enemyId; }

	Vector3 GetPosition() const { return wt_.translate_; }
	Vector3 GetTranslate() const { return wt_.translate_; }
	void SetTranslate(const Vector3& pos) { wt_.translate_ = pos; }
	void AddTranslate(const Vector3& delta) { wt_.translate_ += delta; }

	Vector3 GetSpawnPosition() const { return spawnPosition_; }
	Vector3 GetPatrolTarget() const { return patrolTarget_; }
	void SetPatrolTarget(const Vector3& target) { patrolTarget_ = target; }

	void SetRotationY(float y) { wt_.rotate_.y = y; }
	float GetRotationY() const { return wt_.rotate_.y; }

	void SetPlayer(Player* player) { player_ = player; }
	Player* GetPlayer() const { return player_; }
	Vector3 GetPlayerPosition() const;
	bool HasPlayer() const { return player_ != nullptr; }

	void SetFieldEnemyManager(FieldEnemyManager* manager) { fieldEnemyManager_ = manager; }

	bool HasTriggeredEncounter() const { return hasTriggeredEncounter_; }
	void ResetEncounterTrigger() { ResetEncounterState(); }
	void ResetEncounterState();
	bool CanTriggerEncounter() const { return encounterCooldown_ <= 0.0f && !hasTriggeredEncounter_; }
	void UpdateEncounterCooldown(float dt);
	float GetEncounterCooldown() const { return encounterCooldown_; }

	bool IsActive() const { return logicalState_ != FieldEnemyState::Despawn; }
	void Despawn() { logicalState_ = FieldEnemyState::Despawn; }

	std::string GetSpawnId() const { return spawnId_; }
	void SetSpawnId(const std::string& id) { spawnId_ = id; }

	float GetTakeDamage() const { return takeDamage_; }

private:
	///************************* 内部処理 *************************///

	void TriggerEncounter();
	void TakeDamage(const float damage);

private:
	///************************* メンバ変数 *************************///

	std::unique_ptr<IEnemyState<FieldEnemy>> currentState_;
	float stateTimer_ = 0.0f;
	FieldEnemyState logicalState_ = FieldEnemyState::Patrol;

	FieldEnemyData enemyData_;
	std::string spawnId_;

	Player* player_ = nullptr;
	FieldEnemyManager* fieldEnemyManager_ = nullptr;

	Vector3 spawnPosition_;
	Vector3 patrolTarget_;

	bool hasTriggeredEncounter_ = false;
	float encounterCooldown_ = 0.0f;
	float encounterCooldownDuration_ = 1.0f;

	float takeDamage_ = 0.0f;

	// スポットライト管理用
	std::string spotLightName_;
};