#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

// OBB（Oriented Bounding Box、方向付き境界ボックス）の定義
struct OBB {
	// 中心座標
	Vector3 center = { 0.0f, 0.0f, 0.0f };

	// 回転（オイラー角）
	Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // radians

	// 半サイズ（幅/高さ/奥行きの半分）
	Vector3 size = { 1.0f, 1.0f, 1.0f };

	// ローカル座標軸（回転後）
	Vector3 orientations[3]; // ← X, Y, Z軸方向 × size

	// ワールド変換行列（必要なら）
	Matrix4x4 worldMatrix;
};