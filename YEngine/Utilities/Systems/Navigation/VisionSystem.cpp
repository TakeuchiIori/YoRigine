#include "VisionSystem.h"
#include "Intersection/Intersection.h"  // ← 数学クラスを呼ぶだけ
#include <Object3D/ObjectManager.h>
#include <Collision/AABB/AABBCollider.h>
#include <Collision/Core/CollisionTypeIdDef.h>


// ============================================================
// from → to の視線が障害物に遮られていないか判定する。
// 遮られていない（見える）場合 true を返す。
// ============================================================
bool VisionSystem::HasLineOfSight(const Vector3& from, const Vector3& to)
{
	ObjectManager* om = ObjectManager::GetInstance();
	if (!om) return true; // マネージャーがない場合は遮蔽物なしとみなす

	for (const auto* obj : om->GetAllActiveObjects()) {
		if (!obj || !obj->collider) continue;

		// 【フィルタリング】視線を遮る特性を持つオブジェクト（壁・ナビ障害物）のみを対象とする
		if (obj->colliderTypeId != CollisionTypeIdDef::kNavObstacle &&
			obj->colliderTypeId != CollisionTypeIdDef::kStaticWall) continue;

		// 【型チェック】現状の視線判定はAABBコライダーのみサポート
		const auto* aabb = dynamic_cast<const AABBCollider*>(obj->collider.get());
		if (!aabb) continue;

		// 【衝突判定】レイが1つでもAABBに接触したら視線が通っていないと即断定
		if (Intersection::IsCollisionSegmentAABB2D(from, to, aabb->GetAABB())) {
			return false;
		}
	}
	return true;
}