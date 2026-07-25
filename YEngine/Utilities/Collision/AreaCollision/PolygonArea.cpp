#include "PolygonArea.h"
#include "MathFunc.h"
#include <cmath>
#include <limits>
#include <algorithm>

// ============================================================
// コンストラクタ
// ============================================================
PolygonArea::PolygonArea()
{
	SetupAutoJson();
}

PolygonArea::PolygonArea(const std::vector<Vector3>& vertices)
	: vertices_(vertices)
{
	SetupAutoJson();
}

// ============================================================
// 初期化
// ============================================================
void PolygonArea::Initialize(const std::vector<Vector3>& vertices)
{
	vertices_     = vertices;
	groundBottom_ = 0.0f;
	groundTop_    = 100.0f;
	isActive_     = true;
	insideTargets_.clear();
	// SetupAutoJson はコンストラクタ済みのためここでは不要
}

// ============================================================
// 頂点削除
// ============================================================
void PolygonArea::RemoveVertex(int index)
{
	if (index >= 0 && index < static_cast<int>(vertices_.size())) {
		vertices_.erase(vertices_.begin() + index);
	}
}

// ============================================================
// Ray Casting 法による内外判定（XZ 平面）
// ============================================================
bool PolygonArea::IsInside(const Vector3& position) const
{
	if (vertices_.size() < 3) return false;

	// Y 方向の範囲チェック
	if (position.y < groundBottom_ || position.y > groundTop_) return false;

	float px = position.x;
	float pz = position.z;
	int crossings = 0;
	int n = static_cast<int>(vertices_.size());

	for (int i = 0; i < n; ++i) {
		float ax = vertices_[i].x;
		float az = vertices_[i].z;
		float bx = vertices_[(i + 1) % n].x;
		float bz = vertices_[(i + 1) % n].z;

		// 辺が pz をまたぐかチェック
		if ((az <= pz && bz > pz) || (bz <= pz && az > pz)) {
			// 交差する X 座標を計算し px より右にあれば交差カウント
			float t = (pz - az) / (bz - az);
			if (px < ax + t * (bx - ax)) {
				++crossings;
			}
		}
	}

	// 奇数回なら内側
	return (crossings % 2) == 1;
}

// ============================================================
// 位置をエリア内にクランプ
// ============================================================
Vector3 PolygonArea::ClampPosition(const Vector3& position) const
{
	Vector3 result = position;

	// Y 方向のクランプ
	result.y = std::max(groundBottom_, std::min(groundTop_, result.y));

	// すでに内側なら変更なし
	if (IsInside(result)) return result;

	// 最も近い辺上の点を探してクランプ
	float   minDistSq = std::numeric_limits<float>::max();
	Vector3 nearest   = result;
	int     n         = static_cast<int>(vertices_.size());

	for (int i = 0; i < n; ++i) {
		Vector3 cp = ClosestPointOnSegment(result,
			vertices_[i],
			vertices_[(i + 1) % n]);
		float dx = cp.x - result.x;
		float dz = cp.z - result.z;
		float d2 = dx * dx + dz * dz;
		if (d2 < minDistSq) {
			minDistSq = d2;
			nearest   = cp;
		}
	}

	result.x = nearest.x;
	result.z = nearest.z;
	return result;
}

// ============================================================
// エリア境界までの距離を取得
// ============================================================
float PolygonArea::GetDistanceFromBoundary(const Vector3& position) const
{
	if (vertices_.empty()) return 0.0f;

	float minDist = std::numeric_limits<float>::max();
	int n = static_cast<int>(vertices_.size());

	for (int i = 0; i < n; ++i) {
		Vector3 cp = ClosestPointOnSegment(position,
			vertices_[i],
			vertices_[(i + 1) % n]);
		float dx   = cp.x - position.x;
		float dz   = cp.z - position.z;
		minDist    = std::min(minDist, std::sqrt(dx * dx + dz * dz));
	}

	// 内側なら正、外側なら負
	return IsInside(position) ? minDist : -minDist;
}

// ============================================================
// エリアの中心座標を取得（頂点の重心）
// ============================================================
Vector3 PolygonArea::GetCenter() const
{
	if (vertices_.empty()) return {};

	Vector3 sum{};
	for (const auto& v : vertices_) {
		sum.x += v.x;
		sum.z += v.z;
	}
	float n = static_cast<float>(vertices_.size());
	return { sum.x / n, 0.0f, sum.z / n };
}

// ============================================================
// デバッグ描画
// ============================================================
void PolygonArea::Draw(YoRigine::Line* line)
{
	if (!line || !isDebugDrawEnabled_) return;

	int n = static_cast<int>(vertices_.size());
	for (int i = 0; i < n; ++i) {
		line->RegisterLine(vertices_[i], vertices_[(i + 1) % n]);
	}
}

// ============================================================
// 線分 a-b 上で p に最も近い点（XZ 平面）
// ============================================================
Vector3 PolygonArea::ClosestPointOnSegment(const Vector3& p,
	const Vector3& a,
	const Vector3& b) const
{
	float dx   = b.x - a.x;
	float dz   = b.z - a.z;
	float lenSq = dx * dx + dz * dz;

	if (lenSq < 1e-8f) return a;

	float t = ((p.x - a.x) * dx + (p.z - a.z) * dz) / lenSq;
	t = std::max(0.0f, std::min(1.0f, t));

	return { a.x + t * dx, p.y, a.z + t * dz };
}
