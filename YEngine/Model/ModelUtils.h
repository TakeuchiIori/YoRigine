#pragma once

// C++
#include "float.h"
#include "Matrix4x4.h"
#include <unordered_set>
#include <assimp/matrix4x4.h>

///************************* モデル関連のユーティリティ *************************///

// Assimp行列をエンジン行列に変換
inline Matrix4x4 ConvertMatrix(const aiMatrix4x4& aiMat) {
	Matrix4x4 mtx;
	mtx.m[0][0] = aiMat.a1; mtx.m[0][1] = aiMat.a2; mtx.m[0][2] = aiMat.a3; mtx.m[0][3] = aiMat.a4;
	mtx.m[1][0] = aiMat.b1; mtx.m[1][1] = aiMat.b2; mtx.m[1][2] = aiMat.b3; mtx.m[1][3] = aiMat.b4;
	mtx.m[2][0] = aiMat.c1; mtx.m[2][1] = aiMat.c2; mtx.m[2][2] = aiMat.c3; mtx.m[2][3] = aiMat.c4;
	mtx.m[3][0] = aiMat.d1; mtx.m[3][1] = aiMat.d2; mtx.m[3][2] = aiMat.d3; mtx.m[3][3] = aiMat.d4;
	return mtx;
}

// エンジン行列をAssimp行列に変換
inline aiMatrix4x4 ConvertMatrix(const Matrix4x4& mat) {
	aiMatrix4x4 aiMat;
	aiMat.a1 = mat.m[0][0]; aiMat.a2 = mat.m[0][1]; aiMat.a3 = mat.m[0][2]; aiMat.a4 = mat.m[0][3];
	aiMat.b1 = mat.m[1][0]; aiMat.b2 = mat.m[1][1]; aiMat.b3 = mat.m[1][2]; aiMat.b4 = mat.m[1][3];
	aiMat.c1 = mat.m[2][0]; aiMat.c2 = mat.m[2][1]; aiMat.c3 = mat.m[2][2]; aiMat.c4 = mat.m[2][3];
	aiMat.d1 = mat.m[3][0]; aiMat.d2 = mat.m[3][1]; aiMat.d3 = mat.m[3][2]; aiMat.d4 = mat.m[3][3];
	return aiMat;
}

// 行列同士の積を計算
inline Matrix4x4 MultiplyMatrix(const Matrix4x4& a, const Matrix4x4& b) {
	return a * b;
}

// ノード名の正規化
inline std::string NormalizeNodeName(const std::string& name) {
	std::string normalized = name;

	const std::string prefix = "mixamorig:";
	if (normalized.find(prefix) == 0) {
		normalized = normalized.substr(prefix.length());
	}

	return normalized;
}

// 無視ノードのリスト
inline std::unordered_set<std::string> ignoreNodes = {
	NormalizeNodeName("Armature"),
	NormalizeNodeName("Retopology_hp_Plane.002"),
	NormalizeNodeName("Body")
};
