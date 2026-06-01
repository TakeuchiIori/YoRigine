// File name: Intersection.cpp
#include "Intersection.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

// ============================================================
// 内部ヘルパー関数
// ============================================================
float Intersection::ProjectOBB(const OBB& obb, const Vector3& axis, const Vector3 axes[3]) {
	return obb.size.x * std::fabs(Dot(axes[0], axis)) +
		obb.size.y * std::fabs(Dot(axes[1], axis)) +
		obb.size.z * std::fabs(Dot(axes[2], axis));
}

OBB Intersection::ConvertAABBToOBB(const AABB& aabb) {
	OBB obb;
	obb.center = (aabb.min + aabb.max) * 0.5f;
	obb.size = (aabb.max - aabb.min) * 0.5f;
	obb.rotation = { 0.0f, 0.0f, 0.0f };
	return obb;
}

// ============================================================
// レイと球の交差判定
// ============================================================
bool Intersection::IsCollision(const Ray& ray, const Sphere& sphere, RaycastHit* outHit) {
	Vector3 m = ray.origin - sphere.center;
	float b = Dot(m, ray.direction);
	float c = Dot(m, m) - (sphere.radius * sphere.radius);

	if (c > 0.0f && b > 0.0f) {
		return false;
	}

	float discriminant = b * b - c;

	if (discriminant < 0.0f) {
		return false;
	}

	float t = -b - std::sqrt(discriminant);

	if (t < 0.0f) {
		t = -b + std::sqrt(discriminant);
	}

	if (t < 0.0f) {
		return false;
	}

	if (outHit) {
		outHit->isHit = true;
		outHit->distance = t;
		outHit->hitPoint = ray.origin + ray.direction * t;
		outHit->normal = Normalize(outHit->hitPoint - sphere.center);
	}

	return true;
}

// ============================================================
// レイとAABBの交差判定
// ============================================================
bool Intersection::IsCollision(const Ray& ray, const AABB& aabb, RaycastHit* outHit) {
	float tMin = 0.0f;
	float tMax = FLT_MAX;
	Vector3 hitNormal = { 0.0f, 0.0f, 0.0f };

	float t1, t2;

	// X軸
	if (std::abs(ray.direction.x) > 1e-6f) {
		t1 = (aabb.min.x - ray.origin.x) / ray.direction.x;
		t2 = (aabb.max.x - ray.origin.x) / ray.direction.x;
		Vector3 normal1 = { -1.0f, 0.0f, 0.0f };
		Vector3 normal2 = { 1.0f, 0.0f, 0.0f };

		if (t1 > t2) {
			std::swap(t1, t2);
			std::swap(normal1, normal2);
		}
		if (t1 > tMin) { tMin = t1; hitNormal = normal1; }
		if (t2 < tMax) { tMax = t2; }
		if (tMin > tMax) return false;
	}
	else if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) {
		return false;
	}

	// Y軸
	if (std::abs(ray.direction.y) > 1e-6f) {
		t1 = (aabb.min.y - ray.origin.y) / ray.direction.y;
		t2 = (aabb.max.y - ray.origin.y) / ray.direction.y;
		Vector3 normal1 = { 0.0f, -1.0f, 0.0f };
		Vector3 normal2 = { 0.0f,  1.0f, 0.0f };

		if (t1 > t2) {
			std::swap(t1, t2);
			std::swap(normal1, normal2);
		}
		if (t1 > tMin) { tMin = t1; hitNormal = normal1; }
		if (t2 < tMax) { tMax = t2; }
		if (tMin > tMax) return false;
	}
	else if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) {
		return false;
	}

	// Z軸
	if (std::abs(ray.direction.z) > 1e-6f) {
		t1 = (aabb.min.z - ray.origin.z) / ray.direction.z;
		t2 = (aabb.max.z - ray.origin.z) / ray.direction.z;
		Vector3 normal1 = { 0.0f, 0.0f, -1.0f };
		Vector3 normal2 = { 0.0f, 0.0f,  1.0f };

		if (t1 > t2) {
			std::swap(t1, t2);
			std::swap(normal1, normal2);
		}
		if (t1 > tMin) { tMin = t1; hitNormal = normal1; }
		if (t2 < tMax) { tMax = t2; }
		if (tMin > tMax) return false;
	}
	else if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) {
		return false;
	}

	if (outHit) {
		outHit->isHit = true;
		outHit->distance = tMin;
		outHit->hitPoint = ray.origin + ray.direction * tMin;
		outHit->normal = hitNormal;
	}

	return true;
}

// ============================================================
// レイとOBBの交差判定
// ============================================================
bool Intersection::IsCollision(const Ray& ray, const OBB& obb, RaycastHit* outHit) {
	float tMin = 0.0f;
	float tMax = FLT_MAX;
	Vector3 p = obb.center - ray.origin;
	Vector3 hitNormal = { 0.0f, 0.0f, 0.0f };

	for (int i = 0; i < 3; ++i) {
		Vector3 axis = Normalize(obb.orientations[i]);
		float e = (i == 0) ? obb.size.x : (i == 1) ? obb.size.y : obb.size.z;

		float f = Dot(axis, ray.direction);
		float h = Dot(axis, p);

		if (std::abs(f) > 1e-6f) {
			float t1 = (h + e) / f;
			float t2 = (h - e) / f;
			Vector3 normal1 = axis;
			Vector3 normal2 = -axis;

			if (t1 > t2) {
				std::swap(t1, t2);
				std::swap(normal1, normal2);
			}

			if (t1 > tMin) {
				tMin = t1;
				hitNormal = normal1;
			}
			if (t2 < tMax) {
				tMax = t2;
			}
			if (tMin > tMax) return false;
		}
		else if (-h - e > 0.0f || -h + e < 0.0f) {
			return false;
		}
	}

	if (outHit) {
		outHit->isHit = true;
		outHit->distance = tMin;
		outHit->hitPoint = ray.origin + ray.direction * tMin;
		outHit->normal = hitNormal;
	}

	return true;
}

// ============================================================
// レイと平面の交差判定
// ============================================================
bool Intersection::IsCollision(const Ray& ray, const Plane& plane, RaycastHit* outHit) {
	float denom = Dot(plane.normal, ray.direction);

	if (std::abs(denom) < 1e-6f) {
		return false;
	}

	float t = (plane.distance - Dot(plane.normal, ray.origin)) / denom;

	if (t < 0.0f) {
		return false;
	}

	if (outHit) {
		outHit->isHit = true;
		outHit->distance = t;
		outHit->hitPoint = ray.origin + ray.direction * t;
		outHit->normal = plane.normal;
	}

	return true;
}

// ============================================================
// XZ平面限定の線分-AABB判定（視線遮蔽・NavMesh用）
// ============================================================
bool Intersection::IsCollisionSegmentAABB2D(const Vector3& from, const Vector3& to, const AABB& aabb)
{
	// 始点から終点へのベクトル（方向と長さの元）を計算
	float dx = to.x - from.x;
	float dz = to.z - from.z;

	// 線分の長さを計算
	float len = std::sqrt(dx * dx + dz * dz);

	// 線分の長さがほぼゼロ（点である）場合は、衝突なしとして早期リターン（ゼロ除算防止）
	if (len < 1e-6f) return false;

	// 方向ベクトルを正規化（長さを1にする）
	dx /= len;
	dz /= len;

	// レイ（直線）がAABBと交差する範囲を表すパラメータ t 
	// 初期値は線分の範囲（0.0 から len まで）に設定
	float tmin = 0.0f;
	float tmax = len;

	// -----------------------------------------------------------------
	// X軸方向の判定
	// -----------------------------------------------------------------
	if (fabsf(dx) < 1e-6f) {
		// 線分がX軸に対して垂直（真縦）な場合：
		// 始点のX座標がAABBのX範囲外にあれば、絶対に衝突しない
		if (from.x < aabb.min.x || from.x > aabb.max.x) return false;
	}
	else {
		// AABBの左右の壁（面）に衝突するタイミング（t1, t2）を計算
		float t1 = (aabb.min.x - from.x) / dx;
		float t2 = (aabb.max.x - from.x) / dx;

		// 常に t1（進入） < t2（退出） になるように順序を揃える
		if (t1 > t2) std::swap(t1, t2);

		// 衝突範囲（区間）を更新
		tmin = std::max(tmin, t1); // 最も遅い進入タイミング
		tmax = std::min(tmax, t2); // 最も早い退出タイミング

		// 進入が退出より遅くなった場合は、X軸の壁に当たる前に通り過ぎている（衝突なし）
		if (tmin > tmax) return false;
	}

	// -----------------------------------------------------------------
	// Z軸方向の判定
	// -----------------------------------------------------------------
	if (fabsf(dz) < 1e-6f) {
		// 線分がZ軸に対して垂直（真横）な場合：
		// 始点のZ座標がAABBのZ範囲外にあれば、絶対に衝突しない
		if (from.z < aabb.min.z || from.z > aabb.max.z) return false;
	}
	else {
		// AABBの手前と奥の壁（面）に衝突するタイミング（t1, t2）を計算
		float t1 = (aabb.min.z - from.z) / dz;
		float t2 = (aabb.max.z - from.z) / dz;

		// 常に t1（進入） < t2（退出） になるように順序を揃える
		if (t1 > t2) std::swap(t1, t2);

		// 衝突範囲（区間）を更新（X軸の結果と重ね合わせる）
		tmin = std::max(tmin, t1);
		tmax = std::min(tmax, t2);

		// X軸とZ軸の共通する衝突区間が存在しない場合は衝突なし
		if (tmin > tmax) return false;
	}

	// 最終的に有効な衝突区間（tmax >= 0）が残っていれば衝突していると判定
	return tmax >= 0.0f;
}

// ============================================================
// Sphere vs Sphere
// ============================================================
bool Intersection::IsCollision(const Sphere& sphereA, const Sphere& sphereB, CollisionResult* outResult) {
	Vector3 diff = sphereA.center - sphereB.center;
	float distSq = LengthSquared(diff);
	float radiusSum = sphereA.radius + sphereB.radius;

	if (distSq <= radiusSum * radiusSum) {
		if (outResult) {
			outResult->isHit = true;
			float dist = std::sqrt(distSq);
			if (dist > 0.0001f) {
				outResult->normal = diff * (1.0f / dist); // BからAへのベクトル
				outResult->penetrationDepth = radiusSum - dist;
			}
			else {
				outResult->normal = { 0.0f, 1.0f, 0.0f };
				outResult->penetrationDepth = radiusSum;
			}
		}
		return true;
	}
	return false;
}

// ============================================================
// Sphere vs AABB
// ============================================================
bool Intersection::IsCollision(const Sphere& sphere, const AABB& aabb, CollisionResult* outResult) {
	return IsCollision(sphere, ConvertAABBToOBB(aabb), outResult);
}

// ============================================================
// Sphere vs OBB
// ============================================================
bool Intersection::IsCollision(const Sphere& sphere, const OBB& obb, CollisionResult* outResult) {
	Matrix4x4 rotMat = MakeRotateMatrixXYZ(obb.rotation);
	Matrix4x4 invRot = TransPose(rotMat);
	Vector3 localCenter = Transform(sphere.center - obb.center, invRot);

	Vector3 closestLocal = Clamp(localCenter, -obb.size, obb.size);
	Vector3 closestWorld = obb.center + Transform(closestLocal, rotMat);

	Vector3 diff = sphere.center - closestWorld;
	float distSq = LengthSquared(diff);

	if (distSq <= sphere.radius * sphere.radius) {
		if (outResult) {
			outResult->isHit = true;
			float dist = std::sqrt(distSq);

			if (dist > 0.0001f) {
				outResult->normal = diff * (1.0f / dist);
				outResult->penetrationDepth = sphere.radius - dist;
			}
			else {
				outResult->normal = { 0.0f, 1.0f, 0.0f };
				outResult->penetrationDepth = sphere.radius;
			}
		}
		return true;
	}
	return false;
}

// ============================================================
// AABB vs AABB
// ============================================================
bool Intersection::IsCollision(const AABB& aabbA, const AABB& aabbB, CollisionResult* outResult) {
	bool hit = (aabbA.min.x <= aabbB.max.x && aabbA.max.x >= aabbB.min.x) &&
		(aabbA.min.y <= aabbB.max.y && aabbA.max.y >= aabbB.min.y) &&
		(aabbA.min.z <= aabbB.max.z && aabbA.max.z >= aabbB.min.z);

	if (hit && outResult) {
		return IsCollision(ConvertAABBToOBB(aabbA), ConvertAABBToOBB(aabbB), outResult);
	}
	return hit;
}

// ============================================================
// OBB vs OBB
// ============================================================
bool Intersection::IsCollision(const OBB& obbA, const OBB& obbB, CollisionResult* outResult) {
	const float EPSILON = 1e-6f;

	Matrix4x4 matA = MakeRotateMatrixXYZ(obbA.rotation);
	Matrix4x4 matB = MakeRotateMatrixXYZ(obbB.rotation);

	Vector3 axes[15];
	axes[0] = { matA.m[0][0], matA.m[1][0], matA.m[2][0] };
	axes[1] = { matA.m[0][1], matA.m[1][1], matA.m[2][1] };
	axes[2] = { matA.m[0][2], matA.m[1][2], matA.m[2][2] };
	axes[3] = { matB.m[0][0], matB.m[1][0], matB.m[2][0] };
	axes[4] = { matB.m[0][1], matB.m[1][1], matB.m[2][1] };
	axes[5] = { matB.m[0][2], matB.m[1][2], matB.m[2][2] };

	int axisIdx = 6;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			axes[axisIdx++] = Cross(axes[i], axes[3 + j]);
		}
	}

	Vector3 distanceVec = obbB.center - obbA.center;
	float minOverlap = FLT_MAX;
	Vector3 minAxis = { 0, 0, 0 };

	for (int i = 0; i < 15; i++) {
		Vector3 axis = axes[i];
		float lenSq = LengthSquared(axis);
		if (lenSq < EPSILON) continue;
		axis = axis * (1.0f / std::sqrt(lenSq));

		float projA = ProjectOBB(obbA, axis, &axes[0]);
		float projB = ProjectOBB(obbB, axis, &axes[3]);

		float distance = std::abs(Dot(distanceVec, axis));
		float overlap = projA + projB - distance;

		if (overlap < 0.0f) {
			return false; // 分離軸が見つかった＝当たっていない
		}

		if (overlap < minOverlap) {
			minOverlap = overlap;
			minAxis = axis;
		}
	}

	if (outResult) {
		if (Dot(distanceVec, minAxis) > 0.0f) {
			minAxis = minAxis * -1.0f;
		}
		outResult->isHit = true;
		outResult->normal = minAxis;
		outResult->penetrationDepth = minOverlap;
	}

	return true;
}

// ============================================================
// AABB vs OBB
// ============================================================
bool Intersection::IsCollision(const AABB& aabb, const OBB& obb, CollisionResult* outResult) {
	return IsCollision(ConvertAABBToOBB(aabb), obb, outResult);
}

// ============================================================
// 線分上で point に最も近い点
// ============================================================
Vector3 Intersection::ClosestPointOnSegment(
	const Vector3& point, const Vector3& a, const Vector3& b)
{
	Vector3 ab = b - a;
	float denom = LengthSquared(ab);
	if (denom < 1e-8f) {
		return a; // 退化線分 → 端点
	}
	float t = Dot(point - a, ab) / denom;
	if (t < 0.0f) t = 0.0f;
	else if (t > 1.0f) t = 1.0f;
	return a + ab * t;
}

// ============================================================
// 2線分の最近接点ペア (Real-Time Collision Detection 5.1.9 に基づく)
// ============================================================
void Intersection::ClosestPointsSegmentSegment(
	const Vector3& p1, const Vector3& q1,
	const Vector3& p2, const Vector3& q2,
	Vector3* closestA, Vector3* closestB)
{
	Vector3 d1 = q1 - p1;
	Vector3 d2 = q2 - p2;
	Vector3 r = p1 - p2;

	float a = LengthSquared(d1);
	float e = LengthSquared(d2);
	float f = Dot(d2, r);

	float s = 0.0f, t = 0.0f;
	const float EPS = 1e-8f;

	if (a <= EPS && e <= EPS) {
		// 両方が点
		if (closestA) *closestA = p1;
		if (closestB) *closestB = p2;
		return;
	}
	if (a <= EPS) {
		// 線分1が点
		s = 0.0f;
		t = f / e;
		if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
	} else {
		float c = Dot(d1, r);
		if (e <= EPS) {
			// 線分2が点
			t = 0.0f;
			s = -c / a;
			if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
		} else {
			float b = Dot(d1, d2);
			float denom = a * e - b * b;

			if (denom != 0.0f) {
				s = (b * f - c * e) / denom;
				if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
			} else {
				s = 0.0f;
			}

			t = (b * s + f) / e;
			if (t < 0.0f) {
				t = 0.0f;
				s = -c / a;
				if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
			} else if (t > 1.0f) {
				t = 1.0f;
				s = (b - c) / a;
				if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
			}
		}
	}

	if (closestA) *closestA = p1 + d1 * s;
	if (closestB) *closestB = p2 + d2 * t;
}

// ============================================================
// Ray vs Capsule
//  - レイの原点が radius 内なら distance=0 で当たり扱い
//  - そうでなければスフィアスイープ(無限長)で円柱部を解いた後、両端の球チェック
// ============================================================
bool Intersection::IsCollision(const Ray& ray, const Capsule& capsule, RaycastHit* outHit)
{
	// 円柱軸 d 上を回転投影せず、軸方向の最も近い点を解析的に求める。
	Vector3 axis = capsule.end - capsule.start;
	float axisLenSq = LengthSquared(axis);
	if (axisLenSq < 1e-8f) {
		// 退化 → 球として扱う
		Sphere s{ capsule.start, capsule.radius };
		return IsCollision(ray, s, outHit);
	}

	// 原点 - start を分解
	Vector3 m = ray.origin - capsule.start;
	float md = Dot(m, axis);
	float nd = Dot(ray.direction, axis);
	float mm = Dot(m, m);
	float nn = Dot(ray.direction, ray.direction);
	float mn = Dot(m, ray.direction);
	float a = axisLenSq * nn - nd * nd;
	float k = mm - capsule.radius * capsule.radius;
	float c = axisLenSq * k - md * md;

	float bestT = FLT_MAX;
	bool hit = false;
	Vector3 hitPos;
	Vector3 hitNormal;

	if (std::fabs(a) > 1e-8f) {
		float b = axisLenSq * mn - nd * md;
		float disc = b * b - a * c;
		if (disc >= 0.0f) {
			float sqrtDisc = std::sqrt(disc);
			float t = (-b - sqrtDisc) / a;
			if (t >= 0.0f) {
				float s = md + t * nd;
				if (s >= 0.0f && s <= axisLenSq) {
					bestT = t;
					hitPos = ray.origin + ray.direction * t;
					Vector3 onAxis = capsule.start + axis * (s / axisLenSq);
					hitNormal = Normalize(hitPos - onAxis);
					hit = true;
				}
			}
		}
	}

	// 両端の球も判定 (キャップ部分)
	auto checkSphere = [&](const Vector3& center) {
		RaycastHit tmp;
		Sphere sph{ center, capsule.radius };
		if (IsCollision(ray, sph, &tmp)) {
			if (tmp.distance < bestT) {
				bestT = tmp.distance;
				hitPos = tmp.hitPoint;
				hitNormal = tmp.normal;
				hit = true;
			}
		}
	};
	checkSphere(capsule.start);
	checkSphere(capsule.end);

	if (hit && outHit) {
		outHit->isHit = true;
		outHit->distance = bestT;
		outHit->hitPoint = hitPos;
		outHit->normal = hitNormal;
	}
	return hit;
}

// ============================================================
// Capsule vs Sphere
// ============================================================
bool Intersection::IsCollision(const Capsule& cap, const Sphere& sphere, CollisionResult* outResult)
{
	Vector3 closestOnCap = ClosestPointOnSegment(sphere.center, cap.start, cap.end);
	Vector3 diff = closestOnCap - sphere.center;
	float distSq = LengthSquared(diff);
	float radiusSum = cap.radius + sphere.radius;

	if (distSq > radiusSum * radiusSum) return false;

	if (outResult) {
		outResult->isHit = true;
		float dist = std::sqrt(distSq);
		if (dist > 1e-4f) {
			outResult->normal = diff * (1.0f / dist); // Sphere → Capsule (capsuleAをSphereBから押し戻す向き)
			outResult->penetrationDepth = radiusSum - dist;
		} else {
			outResult->normal = { 0.0f, 1.0f, 0.0f };
			outResult->penetrationDepth = radiusSum;
		}
	}
	return true;
}

// ============================================================
// Capsule vs Capsule
// ============================================================
bool Intersection::IsCollision(const Capsule& capA, const Capsule& capB, CollisionResult* outResult)
{
	Vector3 closestA, closestB;
	ClosestPointsSegmentSegment(capA.start, capA.end, capB.start, capB.end, &closestA, &closestB);

	Vector3 diff = closestB - closestA;
	float distSq = LengthSquared(diff);
	float radiusSum = capA.radius + capB.radius;

	if (distSq > radiusSum * radiusSum) return false;

	if (outResult) {
		outResult->isHit = true;
		float dist = std::sqrt(distSq);
		if (dist > 1e-4f) {
			outResult->normal = diff * (1.0f / dist); // A→B方向（Aを押す向き）
			outResult->penetrationDepth = radiusSum - dist;
		} else {
			outResult->normal = { 0.0f, 1.0f, 0.0f };
			outResult->penetrationDepth = radiusSum;
		}
	}
	return true;
}

// ============================================================
// Capsule vs OBB (OBBのローカル空間に変換してから解析)
// ============================================================
bool Intersection::IsCollision(const Capsule& cap, const OBB& obb, CollisionResult* outResult)
{
	// OBBローカル空間にキャプセル軸を変換
	Matrix4x4 rotMat = MakeRotateMatrixXYZ(obb.rotation);
	Matrix4x4 invRot = TransPose(rotMat);

	Vector3 localStart = Transform(cap.start - obb.center, invRot);
	Vector3 localEnd   = Transform(cap.end   - obb.center, invRot);

	// ローカル空間で「線分 vs AABB(±obb.size)」の最近接点を求める。
	// 解析的に厳密解は重いので、線分を細かくサンプリングして最小距離を取る近似で代用。
	// 反復で精度を上げる。
	const int kSamples = 16;
	Vector3 bestOnSeg = localStart;
	Vector3 bestOnBox = Clamp(localStart, -obb.size, obb.size);
	float bestDistSq = LengthSquared(bestOnSeg - bestOnBox);

	for (int i = 1; i <= kSamples; ++i) {
		float t = static_cast<float>(i) / static_cast<float>(kSamples);
		Vector3 onSeg = localStart + (localEnd - localStart) * t;
		Vector3 onBox = Clamp(onSeg, -obb.size, obb.size);
		float d2 = LengthSquared(onSeg - onBox);
		if (d2 < bestDistSq) {
			bestDistSq = d2;
			bestOnSeg = onSeg;
			bestOnBox = onBox;
		}
	}

	// 局所最近接点をワールドに戻し、Sphere-AABB 風の判定で詰める
	Vector3 worldOnSeg = obb.center + Transform(bestOnSeg, rotMat);
	Vector3 worldOnBox = obb.center + Transform(bestOnBox, rotMat);
	Vector3 diff = worldOnSeg - worldOnBox;
	float distSq = LengthSquared(diff);

	if (distSq > cap.radius * cap.radius) return false;

	if (outResult) {
		outResult->isHit = true;
		float dist = std::sqrt(distSq);
		if (dist > 1e-4f) {
			outResult->normal = diff * (1.0f / dist); // OBB→Capsule（Capsuleを押す向き）
			outResult->penetrationDepth = cap.radius - dist;
		} else {
			outResult->normal = { 0.0f, 1.0f, 0.0f };
			outResult->penetrationDepth = cap.radius;
		}
	}
	return true;
}

// ============================================================
// Capsule vs AABB
// ============================================================
bool Intersection::IsCollision(const Capsule& cap, const AABB& aabb, CollisionResult* outResult)
{
	return IsCollision(cap, ConvertAABBToOBB(aabb), outResult);
}