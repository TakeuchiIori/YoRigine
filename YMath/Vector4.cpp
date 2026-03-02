#include "Vector4.h"

Vector4 lerp(const Vector4& a, const Vector4& b, float t) {
    return Vector4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

Vector4 Transform(const Vector4& vector, const Matrix4x4& matrix)
{
	Vector4 result;

	// 入力ベクトル (vector) の成分
	float x = vector.x;
	float y = vector.y;
	float z = vector.z;
	float w = vector.w;

	// 行列 (matrix) の m[行][列]
	const float (*m)[4] = matrix.m;

	// x' の計算 (行列の第0列を使用)
	result.x = x * m[0][0] + y * m[1][0] + z * m[2][0] + w * m[3][0];

	// y' の計算 (行列の第1列を使用)
	result.y = x * m[0][1] + y * m[1][1] + z * m[2][1] + w * m[3][1];

	// z' の計算 (行列の第2列を使用)
	result.z = x * m[0][2] + y * m[1][2] + z * m[2][2] + w * m[3][2];

	// w' の計算 (行列の第3列を使用)
	result.w = x * m[0][3] + y * m[1][3] + z * m[2][3] + w * m[3][3];

	return result;
}
