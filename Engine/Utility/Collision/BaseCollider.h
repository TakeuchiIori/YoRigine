#pragma once
// Engine
#include "WorldTransform./WorldTransform.h"
#include "Object3D./Object3d.h"
#include "Collision./CollisionTypeIdDef.h"
#include "Systems/Camera/Camera.h"
#include "../Graphics/Drawer/LineManager/Line.h"
#include "CollisionTypeIdDef.h"
#include "Loaders/Json/JsonManager.h"
// Math
#include "Vector3.h"
#include "Matrix4x4.h"

enum class ColliderType {
	Sphere,
	AABB,
	OBB,
};

class BaseCollider {
protected:

	void Initialize();

public: // ポリモーフィズム

	using CollisionCallback = std::function<void(BaseCollider* self, BaseCollider* other)>;
	virtual ~BaseCollider();
	// 衝突イベント（コールバック）
	void SetOnEnterCollision(CollisionCallback cb) { enterCallback_ = cb; }
	void SetOnCollision(CollisionCallback cb) { collisionCallback_ = cb; }
	void SetOnExitCollision(CollisionCallback cb) { exitCallback_ = cb; }

	void CallOnEnterCollision(BaseCollider* other) {
		if (enterCallback_) enterCallback_(this, other);
	}
	void CallOnCollision(BaseCollider* other) {
		if (collisionCallback_) collisionCallback_(this, other);
	}
	void CallOnExitCollision(BaseCollider* other) {
		if (exitCallback_) exitCallback_(this, other);
	}

	// 継承先で実装：位置・行列・回転
	virtual Vector3 GetCenterPosition() const = 0;
	virtual Vector3 GetScale() const = 0;
	virtual Vector3 GetAnchorPoint() const = 0;
	virtual Matrix4x4 GetWorldMatrix() const = 0;
	virtual Vector3 GetEulerRotation() const = 0;


public: // アクセッサ

	/// <summary>
	///  種別IDを取得
	/// </summary>
	uint32_t GetTypeID() const { return typeID_; }

	/// <summary>
	///  種別IDを\設定
	/// </summary>
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

	/// <summary>
	/// カメラのセット
	/// </summary>
	/// <param name="camera"></param>
	void SetCamera(Camera* camera) { camera_ = camera; }

	/// <summary>
	/// ワールドトランスフォームのセット
	/// </summary>
	/// <param name="worldTransform"></param>
	void SetTransform(const WorldTransform* worldTransform) { worldTransform_ = worldTransform; }

protected:
	Line* line_ = nullptr;
	const WorldTransform* worldTransform_ = nullptr;
	uint32_t typeID_ = 0u;
private:
	Camera* camera_ = nullptr;
	CollisionCallback enterCallback_;
	CollisionCallback collisionCallback_;
	CollisionCallback exitCallback_;
};
