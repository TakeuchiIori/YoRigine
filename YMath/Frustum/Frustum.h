#pragma once

#include "../Vector3.h"
#include "../Vector4.h"
#include "../Matrix4x4.h"
#include "../MathFunc.h"   // 既存の Plane を使う
#include "../Shape/AABB.h"
#include "../Shape/Sphere.h"

// ============================================================
// Frustum: 6 平面で囲まれた領域。
// すべての平面の内側にあれば内側。
// 平面は (normal, distance) で n·p + d = 0 を表現。
// ExtractFromViewProjection 後は「内側 ⇔ n·p + d ≥ 0」になるよう正規化される。
// ============================================================
struct Frustum {
	Plane planes[6]; // 0:Left, 1:Right, 2:Top, 3:Bottom, 4:Near, 5:Far
};

// ============================================================
// FrustumCorners: 8コーナー (Near 4 + Far 4)
//   並び: NLB, NRB, NRT, NLT, FLB, FRB, FRT, FLT
//   (Near/Far × Left/Right × Bottom/Top)
//   AABB 計算・デバッグ描画・OBB 交差判定で使う
// ============================================================
struct FrustumCorners {
	Vector3 c[8];
};

// ============================================================
// Frustum 抽出 / 判定ユーティリティ
// ============================================================
class FrustumUtil {
public:
	// 行プリオーダー (DirectX) の VP 行列から 6 平面を抽出する。
	// 抽出後、各平面は「内側 = n·p + d >= 0」になるように正規化される。
	static Frustum ExtractFromViewProjection(const Matrix4x4& vp);

	// AABB が視錐台と少しでも重なっていれば true。
	// AABB は世界座標。
	static bool IsAABBVisible(const Frustum& f, const AABB& aabb);

	// 球が視錐台と少しでも重なっていれば true。
	static bool IsSphereVisible(const Frustum& f, const Sphere& s);

	// 点が視錐台の内側なら true。
	static bool IsPointInside(const Frustum& f, const Vector3& p);

	// ────────────────────────────────────────────────────────
	// コーナー / AABB / ファクトリ
	// ────────────────────────────────────────────────────────

	// 8 コーナーを VP の逆変換で求める (NDC 立方体 8 点 → world)
	static FrustumCorners ExtractCornersFromViewProjection(const Matrix4x4& vp);

	// 8 コーナーから world AABB を作る (BroadPhase 用)
	static AABB ComputeAABB(const FrustumCorners& corners);

	// 透視投影 frustum を「位置 + オイラー回転 + FOV(度) + アスペクト + near + far」から構築。
	// 中心は origin、視線方向はオイラー回転を +Z に作用させた向き。
	// (Camera 由来のものを使うなら ExtractFromViewProjection を直接呼ぶこと)
	static void BuildPerspectiveWorld(
		const Vector3& origin,
		const Vector3& eulerRot,
		float fovYDeg, float aspect, float nearZ, float farZ,
		Frustum* outFrustum, FrustumCorners* outCorners);
};
