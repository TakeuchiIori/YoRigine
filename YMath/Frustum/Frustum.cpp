#include "Frustum.h"
#include <cmath>
#include <algorithm>

namespace {
	// Plane を正規化 (normal を単位ベクトルに、distance も同じスケールで割る)
	inline void NormalizePlane(Plane& p) {
		float len = std::sqrt(p.normal.x * p.normal.x + p.normal.y * p.normal.y + p.normal.z * p.normal.z);
		if (len > 1e-8f) {
			float inv = 1.0f / len;
			p.normal.x *= inv;
			p.normal.y *= inv;
			p.normal.z *= inv;
			p.distance *= inv;
		}
	}
}

// ============================================================
// 抽出: row-major (DirectX) 4x4 行列を前提に Gribb-Hartmann を行う。
// 行 i を m_i とすると、各平面は以下:
//   Left   = m3 + m0
//   Right  = m3 - m0
//   Bottom = m3 + m1
//   Top    = m3 - m1
//   Near   = m3 + m2   (DirectX z ∈ [0,1] なら Near = m2)
//   Far    = m3 - m2
// 各平面は (a, b, c, d) で n·p + d = 0 形式。
// 抽出後 d の符号は「内側 n·p + d >= 0」になるよう調整する。
// ============================================================
Frustum FrustumUtil::ExtractFromViewProjection(const Matrix4x4& m) {
	Frustum f{};

	// 行ベクトル取り出し
	auto row = [&](int i) {
		return Vector4{ m.m[0][i], m.m[1][i], m.m[2][i], m.m[3][i] };
	};
	Vector4 r0 = row(0);
	Vector4 r1 = row(1);
	Vector4 r2 = row(2);
	Vector4 r3 = row(3);

	auto setPlane = [](Plane& p, const Vector4& v) {
		p.normal   = { v.x, v.y, v.z };
		p.distance = v.w;
	};

	// Left: r3 + r0
	setPlane(f.planes[0], { r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w });
	// Right: r3 - r0
	setPlane(f.planes[1], { r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w });
	// Top: r3 - r1
	setPlane(f.planes[2], { r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w });
	// Bottom: r3 + r1
	setPlane(f.planes[3], { r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w });
	// Near (DirectX z∈[0,1]): r2 そのもの
	setPlane(f.planes[4], { r2.x, r2.y, r2.z, r2.w });
	// Far: r3 - r2
	setPlane(f.planes[5], { r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w });

	for (int i = 0; i < 6; ++i) NormalizePlane(f.planes[i]);
	return f;
}

bool FrustumUtil::IsAABBVisible(const Frustum& f, const AABB& aabb) {
	// 各平面に対して、AABB のうち平面側に最も遠い「正の頂点 (p-vertex)」が外側にあれば除外。
	for (int i = 0; i < 6; ++i) {
		const Plane& p = f.planes[i];
		// p-vertex: 法線方向に伸ばした最も内側寄りの頂点
		Vector3 pv{
			p.normal.x >= 0.0f ? aabb.max.x : aabb.min.x,
			p.normal.y >= 0.0f ? aabb.max.y : aabb.min.y,
			p.normal.z >= 0.0f ? aabb.max.z : aabb.min.z,
		};
		float d = p.normal.x * pv.x + p.normal.y * pv.y + p.normal.z * pv.z + p.distance;
		if (d < 0.0f) return false;
	}
	return true;
}

bool FrustumUtil::IsSphereVisible(const Frustum& f, const Sphere& s) {
	for (int i = 0; i < 6; ++i) {
		const Plane& p = f.planes[i];
		float d = p.normal.x * s.center.x + p.normal.y * s.center.y + p.normal.z * s.center.z + p.distance;
		if (d < -s.radius) return false;
	}
	return true;
}

bool FrustumUtil::IsPointInside(const Frustum& f, const Vector3& p) {
	for (int i = 0; i < 6; ++i) {
		const Plane& pl = f.planes[i];
		float d = pl.normal.x * p.x + pl.normal.y * p.y + pl.normal.z * p.z + pl.distance;
		if (d < 0.0f) return false;
	}
	return true;
}

// ============================================================
// 8 コーナー: NDC 立方体 8 点 (DirectX z∈[0,1]) を VP^-1 で world に戻す
// 並び: NLB(0), NRB(1), NRT(2), NLT(3), FLB(4), FRB(5), FRT(6), FLT(7)
// ============================================================
FrustumCorners FrustumUtil::ExtractCornersFromViewProjection(const Matrix4x4& vp) {
	Matrix4x4 inv = Inverse(vp);

	// NDC 立方体 (DirectX): x,y ∈ [-1,+1], z ∈ [0,1]
	const Vector3 ndc[8] = {
		{-1, -1, 0}, { 1, -1, 0}, { 1,  1, 0}, {-1,  1, 0},
		{-1, -1, 1}, { 1, -1, 1}, { 1,  1, 1}, {-1,  1, 1},
	};

	FrustumCorners out{};
	for (int i = 0; i < 8; ++i) {
		// 同次変換 (w 除算で透視戻し)
		Vector4 p{ ndc[i].x, ndc[i].y, ndc[i].z, 1.0f };
		Vector4 w = Transform(p, inv);
		float invw = (std::fabs(w.w) > 1e-8f) ? (1.0f / w.w) : 1.0f;
		out.c[i] = { w.x * invw, w.y * invw, w.z * invw };
	}
	return out;
}

AABB FrustumUtil::ComputeAABB(const FrustumCorners& corners) {
	AABB a{ corners.c[0], corners.c[0] };
	for (int i = 1; i < 8; ++i) {
		a.min.x = std::min(a.min.x, corners.c[i].x);
		a.min.y = std::min(a.min.y, corners.c[i].y);
		a.min.z = std::min(a.min.z, corners.c[i].z);
		a.max.x = std::max(a.max.x, corners.c[i].x);
		a.max.y = std::max(a.max.y, corners.c[i].y);
		a.max.z = std::max(a.max.z, corners.c[i].z);
	}
	return a;
}

void FrustumUtil::BuildPerspectiveWorld(
	const Vector3& origin,
	const Vector3& eulerRot,
	float fovYDeg, float aspect, float nearZ, float farZ,
	Frustum* outFrustum, FrustumCorners* outCorners)
{
	// world = scale(1) * rotate * translate
	Matrix4x4 rot = MakeRotateMatrixXYZ(eulerRot);
	Matrix4x4 tr  = MakeTranslateMatrix(origin);
	Matrix4x4 world = Multiply(rot, tr);

	// camera view = inverse(world)
	Matrix4x4 view = Inverse(world);

	// projection
	float fovRad = fovYDeg * (3.14159265359f / 180.0f);
	Matrix4x4 proj = MakePerspectiveFovMatrix(fovRad, aspect, nearZ, farZ);

	Matrix4x4 vp = Multiply(view, proj);

	if (outFrustum)  *outFrustum  = ExtractFromViewProjection(vp);
	if (outCorners)  *outCorners  = ExtractCornersFromViewProjection(vp);
}
