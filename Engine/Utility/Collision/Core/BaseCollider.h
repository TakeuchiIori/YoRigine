#pragma once

// Engine
#include "WorldTransform./WorldTransform.h"
#include "Object3D./Object3d.h"
#include "CollisionTypeIdDef.h"
#include "Systems/Camera/Camera.h"
#include "../Graphics/Drawer/LineManager/Line.h"
#include "Loaders/Json/JsonManager.h"
#include "CollisionDirection.h"
// Math
#include "Vector3.h"
#include "Matrix4x4.h"

/// <summary>
/// コライダーの基本クラス（継承してSphere/AABB/OBBを実装）
/// </summary>
class BaseCollider {
protected:

	/// <summary>
	/// 初期化（派生クラスから呼び出す）
	/// </summary>
	void Initialize();

public:

	using CollisionCallback = std::function<void(BaseCollider* self, BaseCollider* other)>;
	using DirectionalCollisionCallback = std::function<void(BaseCollider* self, BaseCollider* other, HitDirection dir)>;
	virtual ~BaseCollider();

	/*=======================================================

						  コールバック

	=======================================================*/
	void SetOnEnterCollision(CollisionCallback cb) { enterCallback_ = cb; }
	void SetOnCollision(CollisionCallback cb) { collisionCallback_ = cb; }
	void SetOnExitCollision(CollisionCallback cb) { exitCallback_ = cb; }
	void SetOnDirectionCollision(DirectionalCollisionCallback cb) { directionCallback_ = cb; }

	void CallOnEnterCollision(BaseCollider* other) {
		if (enterCallback_) enterCallback_(this, other);
	}
	void CallOnCollision(BaseCollider* other) {
		if (collisionCallback_) collisionCallback_(this, other);
	}
	void CallOnExitCollision(BaseCollider* other) {
		if (exitCallback_) exitCallback_(this, other);
	}
	void CallOnDirectionCollision(BaseCollider* other, HitDirection dir) {
		if (directionCallback_) directionCallback_(this, other,dir);
	}


	/*=======================================================

						  純粋仮想関数

	=======================================================*/
	/// <summary>
	/// 中心座標を取得（派生クラスで実装）
	/// </summary>
	virtual Vector3 GetCenterPosition() const = 0;

	/// <summary>
	/// ワールドトランスフォーム取得（派生クラスで実装）
	/// </summary>
	virtual const WorldTransform& GetWorldTransform() = 0;

	/// <summary>
	/// オイラー角取得（派生クラスで実装）
	/// </summary>
	virtual Vector3 GetEulerRotation() const = 0;

	/// <summary>
	/// JSON読み込み
	/// </summary>
	virtual void InitJson(JsonManager* jsonManager) = 0;

public:

	/*=======================================================

						 アクセッサ

	=======================================================*/

	/// <summary>
	/// コライダータイプID取得
	/// </summary>
	uint32_t GetTypeID() const { return typeID_; }

	/// <summary>
	/// コライダータイプID設定
	/// </summary>
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

	/// <summary>
	/// カメラのセット
	/// </summary>
	void SetCamera(Camera* camera) { camera_ = camera; }

	/// <summary>
	/// ワールドトランスフォームのセット
	/// </summary>
	void SetTransform(const WorldTransform* worldTransform) { wt_ = worldTransform; }

	/// <summary>
	/// 当たり判定の有効/無効を設定
	/// </summary>
	void SetCollisionEnabled(bool enabled) { isCollisionEnabled_ = enabled; }

	/// <summary>
	/// 当たり判定の有効フラグ取得
	/// </summary>
	bool IsCollisionEnabled() const { return isCollisionEnabled_; }

	/// <summary>
	/// 当たり判定そのもの有効フラグ
	/// </summary>
	bool GetIsActive() const { return isActive_; }

	/// <summary>
	/// 当たり判定そのもの有効/無効を切り替え
	/// </summary>
	void SetActive(bool isActive) { isActive_ = isActive; }

protected:

	/*=======================================================

						 継承クラス用

	=======================================================*/
	Line* line_ = nullptr;					// 当たり判定の可視化用ライン
	const WorldTransform* wt_ = nullptr;	// 所有者のワールド変換
	uint32_t typeID_ = 0u;					// 衝突タイプID

public:

	bool isCollisionEnabled_ = true;		// 当たり判定有効フラグ
	Camera* camera_ = nullptr;				// カメラ参照（ライン描画用）

private:

	/*=======================================================

						   非公開

	=======================================================*/
	bool isActive_ = true;					// コライダーが有効かどうか

	// 衝突時コールバック
	CollisionCallback enterCallback_;
	CollisionCallback collisionCallback_;
	CollisionCallback exitCallback_;
	DirectionalCollisionCallback directionCallback_;
};
