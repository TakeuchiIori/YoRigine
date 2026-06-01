#pragma once

// Engine
#include "Sprite/Sprite.h"
#include "Systems/Input./Input.h"
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

// ============================================================
// タイトルシーンのプレイヤークラス
// ============================================================
class DemoPlayer : public BaseObject {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	~DemoPlayer();
	void Initialize(Camera* camera) override;

	void Update() override;

	void Draw() override;
	void DrawAnimation() override;
	void DrawCollision() override;
	void DrawBone(Line& line);
	void DrawShadow();

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
	void TakeDamage(int damege);

public:
	// ============================================================
	// アクセッサ
	// ============================================================
	Vector3 GetWorldPosition();
	void SetPosition(const Vector3& position) { wt_.translate_ = position; }
	Vector3 GetCameraRotation() const;
	Object3d* GetObject3d() { return obj_.get(); }

	void SetFollowCamera(FollowCamera* camera) { followCamera_ = camera; }
	void SetMotion(const std::string& motionName) {
		if (obj_) {
			obj_->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, motionName);
		}
	}

private:
	// ============================================================
	// 内部処理関数
	// ============================================================
	void InitCollision() override;
	void InitJson() override;
	void UpdateWorldTransform();
	void UpdateMotionTime();

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	YoRigine::Input* input_ = nullptr;
	FollowCamera* followCamera_ = nullptr;

	std::unique_ptr<PlayerSword> playerSword_;
	std::unique_ptr<PlayerShield> playerShield_;
	std::unique_ptr<Line> boneLine_;

	Vector3 anchorPoint_ = { 0.0f, -1.0f, 0.0f };

	float motionSpeed_ = 1.0f;
	float preMotionSpeed_ = 1.0f;

	uint32_t maxHP_ = 100;
	uint32_t hp_ = 100;
	bool isAlive_ = true;
	const std::string emitterPath_ = "TitlePlayer";

	float motionSpeed[3] = { 1.0f,1.0f,1.0f };
};