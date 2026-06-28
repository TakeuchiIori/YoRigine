#pragma once
#include "Vector3.h"
#include <map>
#include <vector>
#include <algorithm>

// 座標をキーにするためのラッパー (浮動小数点誤差を許容して比較)
struct Vec3Key {
	float x, y, z;

	// 比較演算子 (std::mapのキーにするために必要)
	bool operator<(const Vec3Key& other) const {
		// わずかな誤差を許容して比較（EPSILON）
		const float EPSILON = 0.0001f;
		if (std::abs(x - other.x) > EPSILON) return x < other.x;
		if (std::abs(y - other.y) > EPSILON) return y < other.y;
		if (std::abs(z - other.z) > EPSILON) return z < other.z;
		return false; // 全て同じとみなす
	}
};

// エッジを一意に識別するキー
struct EdgeKey {
	Vec3Key p1;
	Vec3Key p2;

	// コンストラクタで常に p1 < p2 となるようにソートする
	// これにより、(A, B) というエッジと (B, A) というエッジを同一視できる
	EdgeKey(const Vector3& v1, const Vector3& v2) {
		Vec3Key k1 = { v1.x, v1.y, v1.z };
		Vec3Key k2 = { v2.x, v2.y, v2.z };

		if (k1 < k2) {
			p1 = k1; p2 = k2;
		} else {
			p1 = k2; p2 = k1;
		}
	}

	bool operator<(const EdgeKey& other) const {
		if (p1 < other.p1) return true;
		if (other.p1 < p1) return false;
		return p2 < other.p2;
	}
};

// ヘルパー: 2点の距離が近いか（同一頂点か）
bool IsSamePos(const Vector3& a, const Vector3& b) {
	const float epsilon = 0.0001f;
	return (a.x - b.x) * (a.x - b.x) +
		(a.y - b.y) * (a.y - b.y) +
		(a.z - b.z) * (a.z - b.z) < epsilon;
}