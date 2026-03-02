#pragma once

// Engine
#include "Sprite/Sprite.h"
#include "Systems/Input./Input.h"
#include <Particle/ParticleEmitter.h>
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"

// Math
#include "MathFunc.h"
#include "Vector3.h"

// App
#include "../Generators/Object3D/BaseObject.h"
#include "Movement/PlayerMovement.h"
#include "Combat/PlayerCombat.h"
#include "Weapon/PlayerSword.h"
#include "Weapon/PlayerShield.h"
#include "UI/HealthBar/PlayerHealthBarUI.h"

/// <summary>
/// ゲームシーンのプレイヤークラス
/// </summary>
class Player : public BaseObject {
public:
	///************************* 基本関数 *************************///
	~Player();
	void Initialize(Camera* camera) override;

	void Update() override;

	void Draw() override;
	void DrawAnimation() override;
	void DrawCollision() override;
	void DrawBone(Line& line);
	void DrawShadow();
	void DrawImGui();

	///************************* 当たり判定 *************************///
	void OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);

public:
	///************************* 公開関数 *************************///

	// プレイヤーの状態をリセット
	void Reset();

	// ダメージを受ける関数
	void TakeDamage(int damage);

	// 復活する関数
	void Revive(int reviveHP);

	// 指定方向を向く関数
	void LookAtDirection(const Vector3& direction);
public:
	///************************* アクセッサ *************************///

	// 位置関連
	Vector3 GetWorldPosition();
	void SetPosition(const Vector3& position) { wt_.translate_ = position; }
	Vector3 GetCameraRotation() const;

	// オブジェクト取得
	Object3d* GetObject3d() { return obj_.get(); }
	const Object3d* GetObject3d() const { return obj_.get(); }

	// システム取得
	PlayerMovement* GetMovement() const { return movement_.get(); }
	PlayerCombat* GetCombat() const { return combat_.get(); }
	PlayerSword* GetSword() const { return playerSword_.get(); }
	PlayerShield* GetShield() const { return playerShield_.get(); }

	// カメラ設定
	void SetFollowCamera(FollowCamera* camera) { followCamera_ = camera; }
	FollowCamera* GetFollowCamera() const { return followCamera_; }

	// モーション速度
	float GetMotionSpeed() const { return motionSpeed_; }
	void SetMotionSpeed(float speed) { motionSpeed_ = speed; }
	const float* GetMotionSpeedArray() const { return motionSpeed; }
	float GetMotionSpeed(int index) const {
		if (index >= 0 && index < 3) return motionSpeed[index];
		return 1.0f;
	}
	void SetMotionSpeed(int index, float speed) {
		if (index >= 0 && index < 3) motionSpeed[index] = speed;
	}

	// HP関連
	int32_t GetHP() const { return hp_; }
	uint32_t GetMaxHP() const { return maxHP_; }
	bool IsAlive() const { return isAlive_; }
	void SetHP(int32_t hp) { hp_ = hp; }
	void SetMaxHP(uint32_t maxHP) { maxHP_ = maxHP; }

private:
	///************************* 内部処理関数 *************************///
	void InitStates();
	void InitCombatSystem();
	void HandleCombatInput();

	void InitCollision() override;
	void InitJson() override;

	// モーションの再生時間を更新する関数
	void UpdateMotionTime();

private:
	///************************* メンバ変数 *************************///

	// ポインタ
	YoRigine::Input* input_ = nullptr;
	FollowCamera* followCamera_ = nullptr;

	std::unique_ptr<ParticleEmitter> particleEmitter_;
	std::unique_ptr<PlayerSword> playerSword_;
	std::unique_ptr<PlayerShield> playerShield_;
	std::unique_ptr<Line> boneLine_;
	std::unique_ptr<PlayerMovement> movement_;
	std::unique_ptr<PlayerCombat> combat_;
	std::unique_ptr<ParticleEmitter> testEmitter_;
	std::unique_ptr<PlayerHealthBarUI> healthUI_;

	Vector3 anchorPoint_ = { 0.0f, -1.0f, 0.0f };

	// モーションの再生時間係数
	float motionSpeed_ = 1.0f;
	float preMotionSpeed_ = 1.0f;

	// ステータス
	uint32_t maxHP_ = 400;
	int32_t hp_ = 400;
	bool isAlive_ = true;
	const std::string emitterPath_ = "Player";

	// モーション速度配列（0:Idle, 1:Attack, 2:Guard, 3:Dead）
	float motionSpeed[4] = { 1.0f, 1.0f, 1.0f,1.0f };
};