#pragma once
// Engine
#include "WorldTransform./WorldTransform.h"
#include "Object3D./Object3d.h"
#include "Collision./CollisionTypeIdDef.h"
#include "Systems/Camera/Camera.h"

// Math
#include "Vector3.h"
#include "Matrix4x4.h"

class Collider {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// ワールドトランスフォームの初期化
	/// </summary>
	void UpdateWorldTransform();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(Object3d* obj,Camera* camera);

public: // ポリモーフィズム

	virtual ~Collider() = default;

	// 衝突時に呼ばれる関数
	virtual void OnCollision([[maybe_unused]] Collider* other) {}

	// 中心座標を取得
	virtual Vector3 GetCenterPosition() const = 0;

	virtual Matrix4x4 GetWorldMatrix() const = 0;



public: // アクセッサ

	/// <summary>
	/// ゲッター
	/// </summary>
	float GetRadiusFloat() { return radiusFloat_; }

	Vector3 GetRadiusVector3() { return radiusVector3_; }

	/// <summary>
	/// セッター
	/// </summary>
	void SetRadiusFloat(const float& radius) { radiusFloat_ = radius; }

	/// <summary>
	///  種別IDを取得
	/// </summary>
	uint32_t GetTypeID() const { return typeID_; }

	/// <summary>
	///  種別IDを\設定
	/// </summary>
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }


private:
	// ワールドトランスフォーム
	WorldTransform worldTransform_;
	// 衝突判定
	float radiusFloat_ = 1.5f;
	Vector3 radiusVector3_ = { 1.5f,1.5f,1.5f };
	// 種別ID
	uint32_t typeID_ = 0u;


};
