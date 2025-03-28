#pragma once
// C++
#include <vector>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <algorithm>

// Math
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Quaternion.h"

struct Sphere {
	Vector3 center; // !< 中心点
	float radius;   // !< 半径
};
struct Plane {
	Vector3 normal; // !<法線
	float distance; // !<距離
};
struct Segment {
	Vector3 origin;
	Vector3 diff;
};

struct Triangle {
	Vector3 vertex[3];
};

struct AABB {
	Vector3 min;
	Vector3 max;
};
struct OBB {
	// 中心座標
	Vector3 center = { 0.0f, 0.0f, 0.0f };

	// 回転（オイラー角）
	Quaternion rotation = { 0.0f, 0.0f, 0.0f ,0.0f}; // radians

	// 半サイズ（幅/高さ/奥行きの半分）
	Vector3 size = { 1.0f, 1.0f, 1.0f };

	// ローカル座標軸（回転後）
	Vector3 orientations[3]; // ← X, Y, Z軸方向 × size

	// ワールド変換行列（必要なら）
	Matrix4x4 worldMatrix;

	/// <summary>
	/// 各軸ベクトルを更新（rotationとsizeから）
	/// </summary>
	void UpdateOrientations() {
		Matrix4x4 rotMat = MakeRotateMatrix(rotation);

		orientations[0] = Transform({ 1, 0, 0 }, rotMat) * size.x; // X軸
		orientations[1] = Transform({ 0, 1, 0 }, rotMat) * size.y; // Y軸
		orientations[2] = Transform({ 0, 0, 1 }, rotMat) * size.z; // Z軸
	}

	/// <summary>
	/// ワールド行列を更新（スケール・回転・平行移動）
	/// </summary>
	void UpdateMatrix() {
		Matrix4x4 scaleMat = MakeScaleMatrix(size);
		Matrix4x4 rotMat = MakeRotateMatrix(rotation);
		Matrix4x4 transMat = MakeTranslateMatrix(center);
		worldMatrix = Multiply(scaleMat, Multiply(rotMat, transMat));
	}
};


// ベクトルの内積を計算する関数
float Dot(const Vector3& a, const Vector3& b);

// ベクトルの大きさの二乗を計算する関数
float MagnitudeSquared(const Vector3& v);

// スカラー値の絶対値を計算する関数
float Magnitude(const float& v);

// Vector3の大きさを計算する関数
float Magnitude(const Vector3& v);

// Vector4の大きさを計算する関数
float Magnitude(const Vector4& v);

// スカラー値を正規化する関数
float Normalize(const float& v);

// Vector4を正規化する関数
Vector4 Normalize(const Vector4& v);

// 2つのVector3間の距離を計算する関数
float Distance(const Vector3& a, const Vector3& b);

float Lerp(float a, float b, float t);

// Vector3の長さの二乗を計算する関数
float LengthSquared(const Vector3& v);

// Vector3の長さを計算する関数
float Length(const Vector3& v);

// AABBと点の衝突判定を行う関数
bool IsCollision(const AABB& aabb, const Vector3& point);

// AABBと球の衝突判定を行う関数
bool IsCollision(const AABB& aabb, const Sphere& sphere);

float DegToRad(float degrees);

