#include "MathFunc.h"


float Dot(const Vector3& a, const Vector3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

float MagnitudeSquared(const Vector3& v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}



float Lerp(float a, float b, float t)
{
	return (1.0f - t) * a + t * b;
}



float Magnitude(const float& v) {
	return std::sqrt(v * v);
}

float Magnitude(const Vector3& v) {
	return std::sqrt(MagnitudeSquared(v));
}

float Magnitude(const Vector4& v) {
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

float Normalize(const float& v) {
	return v / std::fabs(v); // 修正：v/v は問題があるため、fabs を使います
}

float Distance(const Vector3& a, const Vector3& b) {
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float dz = b.z - a.z;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}



float LengthSquared(const Vector3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

float Length(const Vector3& v) { return std::sqrt(LengthSquared(v)); }

bool IsCollision(const AABB& aabb, const Vector3& point) {
	if (point.x < aabb.min.x || point.x > aabb.max.x) return false;
	if (point.y < aabb.min.y || point.y > aabb.max.y) return false;
	if (point.z < aabb.min.z || point.z > aabb.max.z) return false;
	return true;
}
bool IsCollision(const AABB& aabb, const Sphere& sphere) {
	Vector3 closestPointInAABB = sphere.center;

	if (sphere.center.x < aabb.min.x) closestPointInAABB.x = aabb.min.x;
	if (sphere.center.y < aabb.min.y) closestPointInAABB.y = aabb.min.y;
	if (sphere.center.z < aabb.min.z) closestPointInAABB.z = aabb.min.z;

	if (sphere.center.x > aabb.max.x) closestPointInAABB.x = aabb.max.x;
	if (sphere.center.y > aabb.max.y) closestPointInAABB.y = aabb.max.y;
	if (sphere.center.z > aabb.max.z) closestPointInAABB.z = aabb.max.z;

	Vector3 difference = closestPointInAABB - sphere.center;
	float distanceSquared = difference.x * difference.x + difference.y * difference.y + difference.z * difference.z;

	return distanceSquared < (sphere.radius * sphere.radius);
}

float DegToRad(float degrees)
{
	return degrees * (std::numbers::pi_v<float> / 180.0f);
}

float RadToDeg(float radius)
{
	return radius * 180.0f / std::numbers::pi_v<float>;
}

float CubicSplineInterpolate(float p0, float p1, float p2, float p3, float t)
{
	// Catmull-Rom スプラインの公式
	float t2 = t * t;
	float t3 = t2 * t;

	return 0.5f * (
		(2.0f * p1) +
		(-p0 + p2) * t +
		(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
		(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
		);
}

