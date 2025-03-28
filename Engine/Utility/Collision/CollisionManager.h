#pragma once
// Engine
#include "Collider.h"
#include "Object3D/Object3d.h"
#include "WorldTransform./WorldTransform.h"

// C++
#include <list>
#include <memory>

#include "MathFunc.h"
// Collision.h
#pragma once
#include "Sphere/SphereCollider.h"
#include "AABB/AABBCollider.h"
#include "OBB/OBBCollider.h"

namespace Collision {

	// Sphere - Sphere
	bool Check(const SphereCollider* a, const SphereCollider* b);

	// Sphere - AABB
	bool Check(const SphereCollider* sphere, const AABBCollider* aabb);

	// Sphere - OBB
	bool Check(const SphereCollider* sphere, const OBBCollider* obb);

	// AABB - AABB
	bool Check(const AABBCollider* a, const AABBCollider* b);

	// OBB - OBB
	bool Check(const OBB& obbA, const OBB& obbB);

	// AABB - OBB
	bool Check(const AABBCollider* aabb, const OBBCollider* obb);

	// OBB - OBB
	bool Check(const OBBCollider* a, const OBBCollider* b);

}

class CollisionManager {
public: // 基本的な関数

	/// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
	static CollisionManager* GetInstance();

	// コンストラクタ
	// デストラクタ
	CollisionManager() = default;
	~CollisionManager() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 当たり判定の更新
	/// </summary>
	void Update();

public:
	/// <summary>
	/// リセット
	/// </summary>
	void Reset();

	/// <summary>
	/// コライダー2つの衝突判定と応答
	/// </summary>
	void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

	/// <summary>
	/// 全ての当たり判定チェック
	/// </summary>
	void CheckAllCollisions();

	/// <summary>
	/// リストに登録
	/// </summary>
	void AddCollider(Collider* collider);

	/// <summary>
	/// コライダーの削除
	/// </summary>
	void RemoveCollider(Collider* collider);

private:

	// コピーコンストラクタと代入演算子を削除して複製を防ぐ
	CollisionManager(const CollisionManager&) = delete;
	CollisionManager& operator=(const CollisionManager&) = delete;

	// コライダー
	std::list<Collider*> colliders_;
	// bool型
	bool isDrawCollider_ = false;
};
