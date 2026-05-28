#pragma once

#include "../Ray/Raycast.h"
#include "../Shape/Sphere.h"
#include "../Shape/AABB.h"
#include "../Shape/OBB.h"
#include "../MathFunc.h"

// ============================================================
// 衝突結果をまとめる構造体
// ============================================================
struct CollisionResult {
	bool isHit = false;
	Vector3 normal = {};
	float penetrationDepth = 0.0f;
};

// ============================================================
// 交差判定クラス
// ============================================================
class Intersection {
public:
	// ============================================================
	// レイキャスト判定
	// ============================================================
	static bool IsCollision(const Ray& ray, const Sphere& sphere, RaycastHit* outHit = nullptr);
	static bool IsCollision(const Ray& ray, const AABB& aabb, RaycastHit* outHit = nullptr);
	static bool IsCollision(const Ray& ray, const OBB& obb, RaycastHit* outHit = nullptr);
	static bool IsCollision(const Ray& ray, const Plane& plane, RaycastHit* outHit = nullptr);


	// ============================================================
	// XZ平面限定の線分-AABB判定（視線遮蔽・NavMesh用）
	// ============================================================
	// Y軸を無視した2D判定。地面移動キャラクターの視線判定に使う。
	// from → to の線分が aabb の XZ 投影と交差する場合 true を返す。
	static bool IsCollisionSegmentAABB2D(const Vector3& from, const Vector3& to,
		const AABB& aabb);


	// ============================================================
	// 形状同士の交差判定（押し戻し情報付き）
	// ============================================================
	static bool IsCollision(const Sphere& sphereA, const Sphere& sphereB, CollisionResult* outResult = nullptr);
	static bool IsCollision(const Sphere& sphere, const AABB& aabb, CollisionResult* outResult = nullptr);
	static bool IsCollision(const Sphere& sphere, const OBB& obb, CollisionResult* outResult = nullptr);
	static bool IsCollision(const AABB& aabbA, const AABB& aabbB, CollisionResult* outResult = nullptr);
	static bool IsCollision(const OBB& obbA, const OBB& obbB, CollisionResult* outResult = nullptr);
	static bool IsCollision(const AABB& aabb, const OBB& obb, CollisionResult* outResult = nullptr);

	// ============================================================
	// ユーティリティ
	// ============================================================
	static OBB ConvertAABBToOBB(const AABB& aabb);

private:
	// ============================================================
	// 内部計算用ヘルパー
	// ============================================================
	static float ProjectOBB(const OBB& obb, const Vector3& axis, const Vector3 axes[3]);
};