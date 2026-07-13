#include "VisionSystem.h"
#include "Intersection/Intersection.h"  // ← 数学クラスを呼ぶだけ
#include "MathFunc.h"
#include <Object3D/ObjectManager.h>
#include <Collision/AABB/AABBCollider.h>
#include <Collision/OBB/OBBCollider.h>
#include <Collision/Core/CollisionTypeIdDef.h>

namespace {
	// OBB の XZ 平面投影に対して、線分 from-to が交差するかを判定する。
	// 主に Y 軸まわりの回転を想定 (壁・障害物の通常配置)。OBB が pitch/roll を
	// 持っていても XZ 上の OBB ローカル軸を使って射影するため、線分が完全に
	// XZ 平面上にある場合の判定としては妥当な近似になる。
	bool SegmentVsOBBxz(const Vector3& from, const Vector3& to, const OBB& obb) {
		// OBB の X / Z 軸 (回転後) を取り出して XZ 平面へ射影
		const Matrix4x4 rotMat = MakeRotateMatrixXYZ(obb.rotation);
		const Vector3 axX = TransformNormal({ 1.0f, 0.0f, 0.0f }, rotMat);
		const Vector3 axZ = TransformNormal({ 0.0f, 0.0f, 1.0f }, rotMat);

		// XZ ベクトル化
		const float xnLen = std::sqrt(axX.x * axX.x + axX.z * axX.z);
		const float znLen = std::sqrt(axZ.x * axZ.x + axZ.z * axZ.z);
		if (xnLen < 1e-6f || znLen < 1e-6f) return false; // 縮退ケースは遮蔽なし扱い

		const float invXn = 1.0f / xnLen;
		const float invZn = 1.0f / znLen;
		// OBB ローカル空間 (XZ) の単位軸
		const Vector3 ux = { axX.x * invXn, 0.0f, axX.z * invXn };
		const Vector3 uz = { axZ.x * invZn, 0.0f, axZ.z * invZn };

		// from / to を OBB 中心からの相対座標 (XZ) に変換し、ローカル軸へ射影
		const float dx = from.x - obb.center.x;
		const float dz = from.z - obb.center.z;
		const float ex = to.x - obb.center.x;
		const float ez = to.z - obb.center.z;

		const Vector3 localFrom = {
			dx * ux.x + dz * ux.z,
			0.0f,
			dx * uz.x + dz * uz.z };
		const Vector3 localTo = {
			ex * ux.x + ez * ux.z,
			0.0f,
			ex * uz.x + ez * uz.z };

		// ローカル空間では軸並行な箱になるので AABB 線分判定を再利用。
		// size は半サイズなので min/max は ±size。
		AABB localBox{};
		localBox.min = { -obb.size.x, -1.0f, -obb.size.z };
		localBox.max = {  obb.size.x,  1.0f,  obb.size.z };
		return Intersection::IsCollisionSegmentAABB2D(localFrom, localTo, localBox);
	}
}


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
		if (!obj->colliderEnabled) continue; // 無効化された障害物は遮らない

		// 【フィルタリング】視線を遮る特性を持つオブジェクト（壁・ナビ障害物）のみを対象とする
		if (obj->colliderTypeId != CollisionTypeIdDef::kNavObstacle &&
			obj->colliderTypeId != CollisionTypeIdDef::kStaticWall) continue;

		// 形状ベースの静的ディスパッチ (dynamic_cast は使わない方針。CollisionManager 等と同様)
		switch (obj->collider->GetShape()) {
		case ColliderShape::AABB: {
			const auto* aabb = static_cast<const AABBCollider*>(obj->collider.get());
			if (Intersection::IsCollisionSegmentAABB2D(from, to, aabb->GetAABB())) {
				return false;
			}
			break;
		}
		case ColliderShape::OBB: {
			// OBB (回転を考慮した XZ 線分判定)
			const auto* obb = static_cast<const OBBCollider*>(obj->collider.get());
			if (SegmentVsOBBxz(from, to, obb->GetOBB())) {
				return false;
			}
			break;
		}
		default:
			// Sphere / Capsule は視線遮蔽として扱わない
			break;
		}
	}
	return true;
}