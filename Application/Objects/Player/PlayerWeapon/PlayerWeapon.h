#pragma once

// Engine
#include "Object3D./Object3d.h"
#include "Systems/Camera/Camera.h"
#include "Systems/Input./Input.h"
#include "WorldTransform./WorldTransform.h"
#include "Collision./BaseCollider.h"
#include "Collision/Sphere/SphereCollider.h"
#include "Collision/AABB/AABBCollider.h"
#include "Collision/OBB/OBBCollider.h"
#include <json.hpp>
using json = nlohmann::json;
// C++
#include <memory>

// Math
#include "MathFunc.h"
#include "Vector3.h" 
#include <Collision/ContactRecord.h>
#include <Collision/Effect.h>

class PlayerWeapon final : public OBBCollider
{
public:
	//==========================================================================//
	//								構造体など								　　	//
	//==========================================================================//
	enum class WeaponState {
		Idle,        // 待機中
		Attacking,   // 攻撃中
		LSwing,		 // 左斜め切り
		RSwing,		 // 右斜め切り
		JumpAttack,	 // ジャンプ攻撃
		Dashing,     // ダッシュ攻撃中
		Cooldown     // クールダウン中
	};

	struct SRTKeyframe {
		float time;                 // キーフレームの時間（0.0～1.0）
		Vector3 position;           // 位置（Translation）
		Vector3 scale;              // スケール（Scale）
		Quaternion rotation;        // 回転（Rotation）
	};

	struct AttackMotion {
		float duration;             // モーションの長さ（秒単位）
		float hitStartTime;         // ヒット判定が有効になる時間
		float hitEndTime;           // ヒット判定が無効になる時間
		std::vector<SRTKeyframe > srtKeyframes; // SRTのキーフレームデータ
	};

public: 
	//==========================================================================//
	//								基本的な関数								　　	//
	//==========================================================================//

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Camera* camera);
	void InitJson();
	void SaveGlobalVariables();

	/// <summary>
	///  更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(Camera* camera); 
	void DrawCollision();

	/// <summary>
	/// デバッグ用UIの描画
	/// </summary>
	void DrawDebugUI();

private: 
	//==========================================================================//
	//								メンバ関数								　　	//
	//==========================================================================//


	/// <summary>
	/// コンボが可能かどうかを判定
	/// </summary>
	bool IsComboAvailable() const;

	// 各モーションの初期化関数

	/// <summary>
	///	 全ての状態を初期化管理
	/// </summary>
	void InitializeState();

	/// <summary>
	/// 通常モーションの初期化
	/// </summary>
	void InitIdle();

	/// <summary>
	/// 攻撃関数の初期化
	/// </summary>
	void InitAttack();

	/// <summary>
	/// 左横振り攻撃
	/// </summary>
	void InitLeftHorizontalSwing();

	/// <summary>
	/// 右横振り攻撃
	/// </summary>
	void InitRightHorizontalSwiwng();

	/// <summary>
	/// ジャンプ攻撃
	/// </summary>
	void InitJumpAttack();

	/// <summary>
	/// ダッシュ攻撃の初期化
	/// </summary>
	void InitDash();

	/// <summary>
	/// クールダウンの初期化
	/// </summary>
	void InitCooldown();


	// 各モーションの更新処理

	/// <summary>
	/// 全状態の更新処理
	/// </summary>
	void UpdateState();

	/// <summary>
	/// 通常モーション（何もしていない時）
	/// </summary>
	/// <param name="deltaTime"></param>
	void IdleMotion(float deltaTime);

	/// <summary>
	/// 攻撃モーションの更新
	/// </summary>
	/// <param name="deltaTime"></param>
	void UpdateAttackMotion(float deltaTime);

	/// <summary>
	/// 左横振り攻撃
	/// </summary>
	void UpdateLeftHorizontalSwing(float deltaTime);

	/// <summary>
	/// 右横振り攻撃
	/// </summary>
	void UpdateRightHorizontalSwiwng(float deltaTime);

	/// <summary>
	/// ジャンプ攻撃
	/// </summary>
	void UpdateJumpAttack(float deltaTime);

	/// <summary>
	/// ダッシュ攻撃の更新処理
	/// </summary>
	/// <param name="deltaTime"></param>
	void UpdateDashMotion(float deltaTime);


	/// <summary>
	/// クールダウンの更新
	/// </summary>
	/// <param name="deltaTime"></param>
	void UpdateCooldown(float deltaTime);

public: // ポリモーフィズム

	/// <summary>
	/// 衝突を検出したら呼び出されるコールバック関数
	/// </summary>
	void OnCollision([[maybe_unused]] BaseCollider* other) override;
	void EnterCollision([[maybe_unused]] BaseCollider* other) override;
	void ExitCollision([[maybe_unused]] BaseCollider* other) override;

	/// <summary>
	/// 中心座標を取得
	/// </summary>
	/// <returns></returns>
	Vector3 GetCenterPosition() const override;

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	Matrix4x4 GetWorldMatrix() const override;


	Vector3 GetEulerRotation() override { return worldTransform_.rotation_; }

	void ApplyGlobalVariables();

public:
	//==========================================================================//
	//								アクセッサ								　　	//
	//==========================================================================//
	
	void SetParent(WorldTransform& worldTransform) { worldTransform_.parent_ = &worldTransform; }

	const WorldTransform& GetWorldTransform() { return worldTransform_; }
	const Vector3& GetScale() { return worldTransform_.scale_; }
	const Vector3& GetRotation() { return worldTransform_.rotation_; }
	const Vector3& GetTranslation() { return worldTransform_.translation_; }

	const bool& GetIsJumpAttack() { return isJumpAttack_; }
	const bool& GetIsDashAttack() { return isDashAttack_; }

private:
	//==========================================================================//
	//								メンバ変数								　　	//
	//==========================================================================//

	// ポインタ
	std::unique_ptr<Object3d> weapon_;
	std::unique_ptr <JsonManager> jsonCollider_;
	Camera* camera_ = nullptr;
	Input* input_ = nullptr;
	// ワールドトランスフォーム
	WorldTransform worldTransform_;
	// 接触記録クラス
	ContactRecord contactRecord_;
	// エフェクト
	std::list<Effect*> effects_;
	bool isUpdate_ = true;

	WeaponState state_ = WeaponState::Idle; // 武器の状態
	std::optional<WeaponState> stateRequest_;
	float idleTime_ = 0.0f;                 // Idle状態の時間経過
	float attackProgress_ = 0.0f;           // 攻撃モーションの進行度（0.0～1.0）
	float cooldownTime_ = 0.5f;             // クールダウンの時間（秒単位）
	float elapsedCooldownTime_ = 0.0f;      // クールダウン経過時間


	const AttackMotion* currentMotion_ = nullptr; // 現在の攻撃モーション
	std::vector<AttackMotion> attackMotions_; // 複数の攻撃モーションを管理

	// 各モーション
	const AttackMotion* dashMotion_ = nullptr; // ダッシュモーション
	const AttackMotion* LSwing_= nullptr; // 左横振り
	const AttackMotion* RSwing_= nullptr; // 右横振り
	const AttackMotion* jumpAttack_ = nullptr; // 右横振り

	bool canCombo_ = false;      // コンボ可能かどうか
	bool isJumpAttack_ = false;	 // ジャンプ攻撃フラグ
	bool isDashAttack_ = false;	 // ダッシュ攻撃フラグ
	float comboWindow_ = 0.5f;   // コンボ入力の猶予時間（秒）
	float elapsedComboTime_ = 0.0f; // コンボ猶予時間の経過時間

};

