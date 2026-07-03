#pragma once
#include "../Generators/Object3D/BaseObject.h"
#include "../IEnemyState.h"
#include "BattleEnemyData.h"
#include "UI/HealthBar/EnemyHealthBarUI.h"


#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <random>

#include <Systems/Animation/ObjectAnimation.h>
#include "Particle/YEmitterGroup.h"

class Player;

///************************* 戦闘用の敵クラス *************************///
class BattleEnemy : public BaseObject {
public:
	///************************* 基本関数 *************************///

	// デストラクタ
	~BattleEnemy();

	// 初期化処理
	void Initialize(Camera* camera) override;

	// 戦闘データをもとに初期化（scale はフィールド敵から引き継ぐ見た目スケール）
	void InitializeBattleData(const BattleEnemyData& data, Vector3 position,
		const Vector3& scale = Vector3(1.0f, 1.0f, 1.0f));

	// 当たり判定初期化
	void InitCollision() override;

	// JSON初期化
	void InitJson() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// プレイヤー敗北時の半透明フェード色更新（インスタンシング描画から呼ぶ）
	void ApplyDeathFade();

	// UI描画
	void DrawUI();

	// 影の処理
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
	void ChangeState(std::unique_ptr<IEnemyState<BattleEnemy>> newState);

	///************************* タイマー制御 *************************///

	// 状態タイマーをリセット
	void ResetStateTimer() { stateTimer_ = 0.0f; }

	// 状態タイマーを加算
	void AddStateTimer(float dt) { stateTimer_ += dt; }

	// 状態タイマーを取得
	float GetStateTimer() const { return stateTimer_; }

	///************************* 行動制御 *************************///

	// 行動可否を設定
	void SetCanAct(bool v) { canAct_ = v; }

	// 行動可能か取得
	bool CanAct() const { return canAct_; }

	// ターゲット位置を持つか設定
	void SetHasTargetPosition(bool v) { hasTargetPosition_ = v; }

	// ターゲット位置を持つか取得
	bool HasTargetPosition() const { return hasTargetPosition_; }

	// 位置を加算
	void AddTranslate(const Vector3& delta) { wt_.translate_ += delta; }

	///************************* 見た目制御 *************************///

	// 敵の色を設定
	void SetColor(const Vector4& c) { if (obj_) obj_->SetMaterialColor(c); }

	///************************* アクセッサ *************************///

	// 現在の論理状態を取得
	BattleEnemyState GetState() const { return logicalState_; }

	// 現在の状態クラスを取得
	IEnemyState<BattleEnemy>* GetCurrentState() const { return currentState_.get(); }

	// 敵データを取得
	const BattleEnemyData& GetEnemyData() const { return enemyData_; }
	BattleEnemyData& GetEnemyData() { return enemyData_; }

	// 生存フラグ
	bool IsAlive() { return isAlive_; }
	void SetIsAlive(bool v) { isAlive_ = v; }

	// ダメージ点滅中か参照
	bool& IsDamageBlinking() { return isDamageBlinking_; }
	// 無敵状態か参照
	bool& IsInvincible() { return isInvincible_; }
	// 現在HPを取得
	int GetCurrentHP() const { return enemyData_.currentHp_; }
	// 最大HPを取得
	int GetMaxHP() const { return enemyData_.maxHp_; }


	// ターゲット位置を取得
	Vector3 GetTargetPosition() const { return targetPosition_; }
	// ターゲット位置を設定
	void SetTargetPosition(const Vector3& pos) { targetPosition_ = pos; hasTargetPosition_ = true; }


	// プレイヤーを設定
	void SetPlayer(Player* player) { player_ = player; }
	// プレイヤーを取得
	Player* GetPlayer() const { return player_; }
	// プレイヤーの位置を取得
	Vector3 GetPlayerPosition() const;

	// 最後に確認したプレイヤー位置を設定
	void SetLastKnownPlayerPosition(const Vector3& pos) { lastKnownPlayerPosition_ = pos; hasValidTarget_ = true; }
	Vector3 GetLastKnownPlayerPosition() const { return lastKnownPlayerPosition_; }


	// 有効なターゲットがあるか
	bool HasValidTarget() const { return hasValidTarget_; }
	// 位置を取得
	Vector3 GetTranslate() const { return wt_.translate_; }
	void SetTranslate(const Vector3& pos) { wt_.translate_ = pos; }
	// Y軸回転を設定
	void SetRotationY(float y) { wt_.rotate_.y = y; }
	// Y軸回転を取得
	float GetRotationY() const { return wt_.rotate_.y; }
	// X軸回転を取得
	float& GetRotationX() { return wt_.rotate_.x; }
	// Z軸回転を取得
	float& GetRotationZ() { return wt_.rotate_.z; }
	// 移動到達閾値を取得
	float GetArrivalThreshold() const { return arrivalThreshold_; }

	// ノックバックデータを取得
	const KnockbackData& GetKnockbackData() { return knockbackData_; }

	// オブジェクト名を取得
	void SetObjectName(const std::string& name) { objectName_ = name; }

	// アニメーション管理クラスを取得
	ObjectAnimation* GetAnimation() { return animation_.get(); }

	// ヒットカウント関連
	int GetConsecutiveHitCount() const { return consecutiveHitCount_; }
	void ResetConsecutiveHitCount() { consecutiveHitCount_ = 0; hitCountResetTimer_ = 0.0f; }

	///************************* 攻撃・エフェクト処理 *************************///

	// 通常攻撃を実行
	void PerformBasicAttack();

	// 死亡エフェクトを再生
	void PlayDeathEffect();

	// 死亡演出（ディゾルブ）パラメータ
	float GetDissolveDuration() const { return dissolveDuration_; }

	// ダメージ点滅更新
	void UpdateBlinking(float dt);

	// ノックバック
	void StartKnockback(const Vector3& direction, float power, float duration);
	void UpdateKnockback(float dt);

	///************************* デバッグ用 *************************///

	// ダメージを与える
	void TakeDamage(int damage);

	// HPを回復
	void Heal(int amount);

private:
	///************************* メンバ変数 *************************///

	// 現在の状態クラス
	std::unique_ptr<IEnemyState<BattleEnemy>> currentState_;

	// 状態タイマー
	float stateTimer_ = 0.0f;

	// 論理状態
	BattleEnemyState logicalState_ = BattleEnemyState::Idle;

	// 戦闘データ
	BattleEnemyData enemyData_;

	// プレイヤーターゲット情報
	Player* player_ = nullptr;
	Vector3 lastKnownPlayerPosition_;
	bool hasValidTarget_ = false;

	// 状態フラグ
	bool canAct_ = true;
	bool hasTargetPosition_ = false;
	bool isInvincible_ = false;
	bool isAlive_ = true;

	// 移動関連
	Vector3 targetPosition_;
	float arrivalThreshold_ = 0.5f;

	// ダメージ点滅管理
	float blinkTimer_ = 0.0f;
	bool isDamageBlinking_ = false;

	// 点滅スピード
	float blinkSpeed_ = 50.0f;

	// 前フレームの位置
	Vector3 previousPosition_{ 0,0,0 }; 
	// 現在の移動ベクトル
	Vector3 currentVelocity_{ 0,0,0 };  

	// フェードスピード
	float fadeSpeed_ = 3.0f;

	// 死亡演出（ディゾルブ）。エディタから JsonManager 経由で調整
	float   dissolveDuration_  = 1.2f;                     // 0→1 まで何秒かけて消えるか
	float   dissolveEdgeWidth_ = 0.08f;                    // エッジ発光帯の幅
	Vector3 dissolveEdgeColor_ = { 1.8f, 0.7f, 0.15f };    // エッジ発光色（オレンジ系）
	float   dissolveNoiseScale_ = 6.0f;                    // ノイズの空間スケール

	//	ノックバックデータ
	KnockbackData knockbackData_;
	std::unique_ptr<EnemyHealthBarUI> healthBarUI_;
	std::string objectName_ = "BattleEnemy"; // デフォルト名

	// アニメーション管理
	std::unique_ptr<ObjectAnimation> animation_;

	// 連続ヒット管理（しきい値・リセット時間は enemyData_.attackParams.counter から取得）
	int   consecutiveHitCount_ = 0;
	float hitCountResetTimer_  = 0.0f;
};
