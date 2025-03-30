#pragma once
// Engine
#include "BaseCollider.h"
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
#include <set>

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

	// Base - Base
	bool Check(BaseCollider* a, BaseCollider* b);

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
	~CollisionManager();

	
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
	void CheckCollisionPair(BaseCollider* a, BaseCollider* b);

	/// <summary>
	/// 全ての当たり判定チェック
	/// </summary>
	void CheckAllCollisions();

	/// <summary>
	/// リストに登録
	/// </summary>
	void AddCollider(BaseCollider* collider);

	/// <summary>
	/// コライダーの削除
	/// </summary>
	void RemoveCollider(BaseCollider* collider);

private:

	// コピーコンストラクタと代入演算子を削除して複製を防ぐ
	CollisionManager(const CollisionManager&) = delete;
	CollisionManager& operator=(const CollisionManager&) = delete;

	// コライダー
	std::list<BaseCollider*> colliders_;
	// 衝突中のペアを記録（片方が削除される場合の処理も必要）
	std::set<std::pair<BaseCollider*, BaseCollider*>> collidingPairs_;

	// bool型
	bool isDrawCollider_ = false;
};


