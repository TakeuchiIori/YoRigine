#pragma once
#include "../BaseEnemy.h"
#include "../IEnemyState.h"
#include "Graphics/Drawer/LineManager/Line.h"
#include <UI/Alert/EnemyAlert.h>
#include <Systems/Navigation/NavPathfinder.h>

#include <vector>
#include <string>
#include <memory>
#include <json.hpp>

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
// JSON上は "Single"/"Group"/"Boss" の文字列で保存する (nlohmann の enum <-> string 変換)。
// これにより battleType 本体を直接シリアライズでき、以前あった battleTypeStr という
// 手動同期の文字列フィールド (enum更新時に更新し忘れて表示とロジックがズレるバグの温床) が不要になる。
NLOHMANN_JSON_SERIALIZE_ENUM(BattleType, {
	{BattleType::Single, "Single"},
	{BattleType::Group,  "Group"},
	{BattleType::Boss,   "Boss"},
})
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
	float pathRefreshInterval = 0.3f; // NavMesh経路再計算間隔（秒）。Chase中にプレイヤーが動いたとき
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
class FieldEnemy : public BaseEnemy {
public:
	///************************* 基本的な関数 *************************///

	FieldEnemy();
	~FieldEnemy() override;

	void Initialize(YoRigine::Camera* camera) override;
	void InitializeFieldData(const FieldEnemyData& data, const Vector3& spawnPosition);
	void InitCollision() override;
	void InitJson() override;
	void Update() override;
	void Draw() override;
	void DrawShadow();
	void DrawCollision() override;
	void DrawLine(YoRigine::Line* line);
	void DrawUI();

	// ライトの有効/無効を強制的に切り替える
	void SetLightActive(bool isActive);

	// コライダーの有効/無効を切り替える（シーン非アクティブ時に他シーンの衝突判定へ漏れないようにする）
	void SetCollisionActive(bool isActive);

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

	// タイマー制御・回転ユーティリティは BaseEnemy が持つ。

	///************************* アクセッサ *************************///

	void ApplyUpdatedData(const FieldEnemyData& data);
	const FieldEnemyData& GetEnemyData() const { return enemyData_; }

	const std::string& GetBattleEnemyId() const { return enemyData_.battleEnemyId; }
	std::vector<std::string> GetBattleEnemyIds() const { return enemyData_.GetBattleEnemyIds(); }
	BattleType GetBattleType() const { return enemyData_.battleType; }
	std::string GetEnemyGroupName() const { return enemyData_.enemyId; }

	Vector3 GetPosition() const { return wt_.translate_; }

	Vector3 GetSpawnPosition() const { return spawnPosition_; }
	Vector3 GetPatrolTarget() const { return patrolTarget_; }
	void SetPatrolTarget(const Vector3& target) { patrolTarget_ = target; }

	// ── Navigation ───────────────────────────────────────────────────────
	void SetNavPathfinder(NavPathfinder* pf) { navPathfinder_ = pf; }
	NavPathfinder* GetNavPathfinder() const { return navPathfinder_; }

	// 現在の経路ウェイポイントリストを取得・設定
	const std::vector<Vector3>& GetNavPath() const { return navPath_; }
	void SetNavPath(const std::vector<Vector3>& path) { navPath_ = path; navPathIndex_ = 0; }
	void ClearNavPath() { navPath_.clear(); navPathIndex_ = 0; }

	// 現在のウェイポイントを取得して進める
	bool HasNavPath() const { return navPathIndex_ < static_cast<int>(navPath_.size()); }
	Vector3 GetCurrentWaypoint() const;
	void AdvanceWaypoint() { if (HasNavPath()) ++navPathIndex_; }

	// 経路再計算タイマー
	void UpdatePathRefreshTimer(float dt) { pathRefreshTimer_ += dt; }
	bool ShouldRefreshPath(float interval = 0.3f) const { return pathRefreshTimer_ >= interval; }
	void ResetPathRefreshTimer() { pathRefreshTimer_ = 0.0f; }

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

	// 背後からの奇襲で与えたダメージ。HPは減らさず、バトル開始時に持ち越すための記録値。
	float GetCarryOverDamage() const { return carryOverDamage_; }

private:
	///************************* 内部処理 *************************///

	void TriggerEncounter();
	void SetCarryOverDamage(float damage) { carryOverDamage_ = damage; }

private:
	///************************* メンバ変数 *************************///

	std::unique_ptr<IEnemyState<FieldEnemy>> currentState_;
	FieldEnemyState logicalState_ = FieldEnemyState::Patrol;
	std::unique_ptr<EnemyAlert> alertUI_;

	FieldEnemyData enemyData_;
	std::string spawnId_;

	FieldEnemyManager* fieldEnemyManager_ = nullptr;

	Vector3 spawnPosition_;
	Vector3 patrolTarget_;

	bool hasTriggeredEncounter_ = false;
	float encounterCooldown_ = 0.0f;
	float encounterCooldownDuration_ = 1.0f;

	float carryOverDamage_ = 0.0f;

	// スポットライト管理用
	std::string spotLightName_;

	// ── Navigation メンバ ────────────────────────────────────────────────
	NavPathfinder* navPathfinder_ = nullptr;
	std::vector<Vector3> navPath_;    // 現在の経路ウェイポイント列
	int   navPathIndex_ = 0;         // 次に向かうウェイポイントのインデックス
	float pathRefreshTimer_ = 0.0f;   // 経路再計算タイマー
};