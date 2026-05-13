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

// ============================================================
// ゲームシーンのプレイヤークラス
// ============================================================
class Player : public BaseObject {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	~Player();
	void Initialize(Camera* camera) override;
	void Update() override;
	void Draw() override;
	void DrawAnimation() override;
	void DrawCollision() override;
	void DrawBone(Line& line);
	void DrawShadow();
	void DrawImGui();
	void DrawVfx();

	// ============================================================
	// 当たり判定
	// ============================================================
	void OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other);
	void OnDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);
	void OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, [[maybe_unused]] HitDirection dir);

public:
	// ============================================================
	// 公開関数
	// ============================================================
	void Reset();
	void TakeDamage(int damage);
	void Revive(int reviveHP);
	void SetInitialPosition();

	bool IsAttackPressedA() const;
	bool IsAttackPressedB() const;

public:
	// ============================================================
	// アクセッサ
	// ============================================================
	Vector3 GetWorldPosition();
	void SetPosition(const Vector3& position) { wt_.translate_ = position; }
	Vector3 GetCameraRotation() const;

	Object3d* GetObject3d() { return obj_.get(); }
	const Object3d* GetObject3d() const { return obj_.get(); }

	PlayerMovement* GetMovement() const { return movement_.get(); }
	PlayerCombat* GetCombat() const { return combat_.get(); }
	PlayerSword* GetSword() const { return playerSword_.get(); }
	PlayerShield* GetShield() const { return playerShield_.get(); }

	void SetFollowCamera(FollowCamera* camera) { followCamera_ = camera; }
	FollowCamera* GetFollowCamera() const { return followCamera_; }

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

	int32_t GetHP() const { return hp_; }
	uint32_t GetMaxHP() const { return maxHP_; }
	bool IsAlive() const { return isAlive_; }
	void SetHP(int32_t hp) { hp_ = hp; }
	void SetMaxHP(uint32_t maxHP) { maxHP_ = maxHP; }

private:
	// ============================================================
	// 内部処理関数
	// ============================================================
	void InitCollision() override;
	void InitJson() override;
	void InitStates();
	void InitCombatSystem();

	void HandleCombatInput();
	void UpdateMotionTime();
	void LookAtDirection(const Vector3& direction);

private:
	// ============================================================
	// メンバ変数
	// ============================================================
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

	float motionSpeed_ = 1.0f;
	float preMotionSpeed_ = 1.0f;

	uint32_t maxHP_ = 400;
	int32_t hp_ = 400;
	bool isAlive_ = true;
	const std::string emitterPath_ = "Player";

	float motionSpeed[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};