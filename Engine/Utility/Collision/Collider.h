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

class Collider {
public:

	void Initialize();

public: // ポリモーフィズム


	virtual ~Collider();
	virtual void InitJson(JsonManager* jsonManager) = 0;
	// 衝突中に呼ばれる関数
	virtual void OnCollision([[maybe_unused]] Collider* other) {}
	// 衝突した瞬間に呼ばれる関数
	virtual void EnterCollision([[maybe_unused]] Collider* other) {}
	// 衝突から離れた瞬間に呼ばれる関数
	virtual void ExitCollision([[maybe_unused]] Collider* other) {}

	virtual Vector3 GetCenterPosition() const = 0;
	virtual Matrix4x4 GetWorldMatrix() const = 0;



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

protected:
	Line* line_ = nullptr;
	Object3d* object3d_ = nullptr;
	
	//JsonManager* jsonManager_ = nullptr;
	// 種別ID
	uint32_t typeID_ = 0u;

private:
	Camera* camera_ = nullptr;
};
