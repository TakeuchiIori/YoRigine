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