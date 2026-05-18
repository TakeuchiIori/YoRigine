#include "MathFunc.h"
#include "../YEngine/Core/WinApp/WinApp.h"


// ============================================================
// ベクトルの内積を計算する関数
// ============================================================
float Dot(const Vector3& a, const Vector3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// ============================================================
// Vector3の大きさの二乗を計算する関数
// ============================================================
float MagnitudeSquared(const Vector3& v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

// ============================================================
// 線形補間（float型）
// ============================================================
float Lerp(float a, float b, float t)
{
	return (1.0f - t) * a + t * b;
}

// ============================================================
// スカラー値の絶対値を計算する関数
// ============================================================
float Magnitude(const float& v) {
	return std::sqrt(v * v);
}

// ============================================================
// Vector3の大きさを計算する関数
// ============================================================
float Magnitude(const Vector3& v) {
	return std::sqrt(MagnitudeSquared(v));
}

// ============================================================
// Vector4の大きさを計算する関数
// ============================================================
float Magnitude(const Vector4& v) {
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

// ============================================================
// スカラー値を正規化する関数
// ============================================================
float Normalize(const float& v) {
	return v / std::fabs(v); // 修正：v/v は問題があるため、fabs を使います
}

// ============================================================
// 2つのVector3間の距離を計算する関数
// ============================================================
float Distance(const Vector3& a, const Vector3& b) {
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float dz = b.z - a.z;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}


// ============================================================
// Vector3の長さの二乗を計算する関数
// ============================================================
float LengthSquared(const Vector3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

// ============================================================
// Vector3の長さを計算する関数
// ============================================================
float Length(const Vector3& v) { return std::sqrt(LengthSquared(v)); }

// ============================================================
// 度数からラジアン
// ============================================================
float DegToRad(float degrees)
{
	return degrees * (std::numbers::pi_v<float> / 180.0f);
}

// ============================================================
// ラジアンから度数
// ============================================================
float RadToDeg(float radius)
{
	return radius * 180.0f / std::numbers::pi_v<float>;
}

// ============================================================
// スプライン補間（float型）
// ============================================================
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

// ============================================================
// ワールド座標をスクリーン座標に変換する関数
// ============================================================
namespace Coordinate {
	std::optional<ScreenProjectionResult> WorldToScreen(const Vector3& worldPos, const Matrix4x4& viewProjectionMatrix, float referenceDistance, float maxScale)
	{
		// 同次クリップ空間への変換
		Vector4 clipSpacePos = Transform({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, viewProjectionMatrix);

		// カメラの後ろ（w <= 0）なら無効
		if (clipSpacePos.w <= 0.0f) {
			return std::nullopt;
		}

		ScreenProjectionResult result;

		// スケール計算
		result.distanceScale = referenceDistance / clipSpacePos.w;
		result.distanceScale = std::min(result.distanceScale, maxScale);

		// 透視除算
		clipSpacePos.x /= clipSpacePos.w;
		clipSpacePos.y /= clipSpacePos.w;

		// スクリーン座標変換
		result.screenPos.x = (clipSpacePos.x * 0.5f + 0.5f) * (float)WinApp::kClientWidth;
		result.screenPos.y = (-clipSpacePos.y * 0.5f + 0.5f) * (float)WinApp::kClientHeight;
		result.screenPos.z = 0.0f;

		return result;
	}
}
