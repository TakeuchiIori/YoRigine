#pragma once

#include "../Ray/Raycast.h"
#include "../Shape/Sphere.h"
#include "../Shape/AABB.h"
#include "../Shape/OBB.h"
#include "../Shape/Capsule.h"
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
	static bool IsCollision(const Ray& ray, const Capsule& capsule, RaycastHit* outHit = nullptr);


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

	// ── Capsule ────────────────────────────────────────────────
	// 法線・侵入深度は基本「A→Bを押し戻す向き」で返す。
	// Sphereと組ませる時はSphereをA扱いし、CollisionManager側で反転処理が必要なら反転する。
	static bool IsCollision(const Capsule& capA,  const Capsule& capB,  CollisionResult* outResult = nullptr);
	static bool IsCollision(const Capsule& cap,   const Sphere&  sphere,CollisionResult* outResult = nullptr);
	static bool IsCollision(const Capsule& cap,   const AABB&    aabb,  CollisionResult* outResult = nullptr);
	static bool IsCollision(const Capsule& cap,   const OBB&     obb,   CollisionResult* outResult = nullptr);

	// ============================================================
	// ユーティリティ
	// ============================================================
	static OBB ConvertAABBToOBB(const AABB& aabb);

	// 2線分の最近接点ペアを返す。closestA/closestB はそれぞれの線分上の最近接点。
	static void ClosestPointsSegmentSegment(
		const Vector3& p1, const Vector3& q1,
		const Vector3& p2, const Vector3& q2,
		Vector3* closestA, Vector3* closestB);

	// 線分上で point に最も近い点を返す。
	static Vector3 ClosestPointOnSegment(
		const Vector3& point, const Vector3& a, const Vector3& b);

private:
	// ============================================================
	// 内部計算用ヘルパー
	// ============================================================
	static float ProjectOBB(const OBB& obb, const Vector3& axis, const Vector3 axes[3]);
};