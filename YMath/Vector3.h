#pragma once
// C++
#include <cmath>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <algorithm>


struct Vector3 final {
	float x;
	float y;
	float z;

	// コンストラクタ
	Vector3() : x(0), y(0), z(0) {}
	Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

	// 加算演算子
	Vector3 operator+(const Vector3& other) const {
		return { x + other.x, y + other.y, z + other.z };
	}
	Vector3& operator+=(const Vector3& other) {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}
	Vector3& operator+=(float scalar) {
		x += scalar;
		y += scalar;
		z += scalar;
		return *this;
	}

	// 減算演算子
	Vector3 operator-(const Vector3& other) const {
		return { x - other.x, y - other.y, z - other.z };
	}
	Vector3 operator-() const {
		return Vector3(-x, -y, -z);
	}
	Vector3& operator-=(const Vector3& other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}
	Vector3& operator-=(float scalar) {
		x -= scalar;
		y -= scalar;
		z -= scalar;
		return *this;
	}
	friend Vector3 operator-(float scalar, const Vector3& vec) {
		return { scalar - vec.x, scalar - vec.y, scalar - vec.z };
	}

	// 乗算演算子
	Vector3 operator*(float scalar) const {
		return { x * scalar, y * scalar, z * scalar };
	}
	Vector3& operator*=(float scalar) {
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}
	friend Vector3 operator*(float scalar, const Vector3& vec) {
		return { vec.x * scalar, vec.y * scalar, vec.z * scalar };
	}

	// 割り算演算子
	Vector3 operator/(float scalar) const {
		return { x / scalar, y / scalar, z / scalar };
	}

	Vector3& operator/=(float scalar) {
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}

	// ベクトルの外積を計算する関数
	static Vector3 Cross(const Vector3& a, const Vector3& b) {
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}

	// ベクトルの長さを計算する関数
	static float Length(const Vector3& v) {
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	// ベクトルを正規化する関数
	static Vector3 Normalize(const Vector3& v) {
		float length = Length(v);
		if (length == 0.0f) {
			return Vector3(0.0f, 0.0f, 0.0f); // ゼロベクトルはそのまま返す
		}
		return v / length;
	}

	// ベクトルの内積を計算する関数
	static float Dot(const Vector3& a, const Vector3& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	// ゼロベクトルかを確認する関数
	bool IsZero() const {
		return x == 0.0f && y == 0.0f && z == 0.0f;
	}

	///************************* メンバ関数（インスタンスから直接） *************************///

	// インスタンスの長さを取得
	float Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	// インスタンスを正規化して返す
	Vector3 Normalize() const {
		float len = Length();
		if (len == 0.0f) {
			return Vector3(0.0f, 0.0f, 0.0f);
		}
		return Vector3(x / len, y / len, z / len);
	}

	// 内積を計算
	float Dot(const Vector3& other) const {
		return x * other.x + y * other.y + z * other.z;
	}

	// 外積を計算
	Vector3 Cross(const Vector3& other) const {
		return Vector3(
			y * other.z - z * other.y,
			z * other.x - x * other.z,
			x * other.y - y * other.x
		);
	}
};

// Vector3型同士を等値比較する演算子
inline bool operator==(const Vector3& lhs, const Vector3& rhs) {
	return (lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.z == rhs.z);
}

// Vector3型同士を非等値比較する演算子
inline bool operator!=(const Vector3& lhs, const Vector3& rhs) {
	return !(lhs == rhs);
}


// 位置・回転・スケールを保持する EulerTransform 構造体
struct EulerTransform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

//============================================================================//



// ベクトルにスカラーを掛け算する関数
Vector3 Multiply(const Vector3& v, float scalar);

// Vector3を正規化する関数（グローバル関数版）
Vector3 Normalize(const Vector3& vec);
Vector3 Normalize(Vector3& vec);

// Catmull-Romスプライン補間を用いて曲線上の点を計算する関数
Vector3 CatmullRomSpline(const std::vector<Vector3>& controlPoints, float t);
// Catmull-Romスプラインのポイントを生成する関数
std::vector<Vector3> GenerateCatmullRomSplinePoints(const std::vector<Vector3>& controlPoints, size_t segmentCount);
// ご提供
Vector3 CatmullRomInterpolation(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
Vector3 CatmullRomPosition(const std::vector<Vector3>& points, float t);

// Vector3同士の加算を行う関数
Vector3 Add(const Vector3& v1, const Vector3& v2);

// Vector3同士の減算を行う関数
Vector3 Subtract(const Vector3& v1, const Vector3& v2);

// ベクトルのクロス積を計算する関数
Vector3 Cross(const Vector3& v1, const Vector3& v2);

Vector3 Lerp(const Vector3& a, const Vector3& b, float t);

Vector3 CubicSplineInterpolate(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

Vector3 lerp(const Vector3& a, const Vector3& b, float t);
float lerp(float a, float b, float t);

Vector3 Slerp(const Vector3& v0, const Vector3& v1, float t);
std::vector<double> CubicSplineInterpolation(
	const std::vector<double>& xData,
	const std::vector<double>& yData,
	const std::vector<double>& xQuery
);

Vector3 Clamp(const Vector3& v, const Vector3& min, const Vector3& max);

// Blender → DirectX変換（Position）
Vector3 ConvertPosition(const Vector3& pos);

Vector3 GetEulerAnglesFromToDirection(const Vector3& from, const Vector3& to);