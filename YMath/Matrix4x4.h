#pragma once
// C++
#include <assert.h>
#include <cmath>

// Math
#include "Vector3.h"
#include <initializer_list>
#include <stdexcept>

// DX
#include <DirectXMath.h>


struct Matrix4x4 {
	float m[4][4];

	// デフォルトコンストラクタ
	Matrix4x4() : m{ {0.0f} } {}

	Matrix4x4(std::initializer_list<float> list);

	// 行列の加算
	Matrix4x4 operator+(const Matrix4x4& other) const {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				result.m[i][j] = m[i][j] + other.m[i][j];
		return result;
	}

	// 行列の減算
	Matrix4x4 operator-(const Matrix4x4& other) const {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				result.m[i][j] = m[i][j] - other.m[i][j];
		return result;
	}

	// 行列の乗算
	Matrix4x4 operator*(const Matrix4x4& other) const {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = 0.0f;
				for (int k = 0; k < 4; ++k) {
					result.m[i][j] += m[i][k] * other.m[k][j];
				}
			}
		}
		return result;
	}

	// スカラー倍
	Matrix4x4 operator*(float scalar) const {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				result.m[i][j] = m[i][j] * scalar;
		return result;
	}

	// 加算代入
	Matrix4x4& operator+=(const Matrix4x4& other) {
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				m[i][j] += other.m[i][j];
		return *this;
	}

	// 減算代入
	Matrix4x4& operator-=(const Matrix4x4& other) {
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				m[i][j] -= other.m[i][j];
		return *this;
	}

	// 乗算代入
	Matrix4x4& operator*=(const Matrix4x4& other) {
		*this = *this * other;
		return *this;
	}

	// スカラー倍代入
	Matrix4x4& operator*=(float scalar) {
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				m[i][j] *= scalar;
		return *this;
	}




};

inline bool IsEqual(const Matrix4x4& lhs, const Matrix4x4& rhs, float epsilon = 1e-6f)
{
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			if (std::abs(lhs.m[row][col] - rhs.m[row][col]) > epsilon) {
				return false;
			}
		}
	}
	return true;
}


	inline bool operator==(const Matrix4x4& lhs, const Matrix4x4& rhs)
	{
		return IsEqual(lhs, rhs);
	}


struct UVTransform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};
// 1. 行列の加法 
Matrix4x4 Add(Matrix4x4 matrix1, Matrix4x4 matrix2);
// 2. 行列の減法 
Matrix4x4 Subtract(Matrix4x4 matrix1, Matrix4x4 matrix2);
// 3. 行列の積 
Matrix4x4 Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2);
// 4. 逆行列
Matrix4x4 Inverse(const Matrix4x4& m);
// 5. 転置行列 
Matrix4x4 TransPose(const Matrix4x4& matrix);
// 6. 単位行列 
Matrix4x4 MakeIdentity4x4();
// 7. 拡大縮小行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale);

// 8. 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

// 9. 座標変換
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
// ベクトル変換データ
Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m);
// 10.回転行列
//    X軸回転行列
Matrix4x4 MakeRotateMatrixX(float radian);
//    Y軸回転行列
Matrix4x4 MakeRotateMatrixY(float radian);
//    Z軸回転行列
Matrix4x4 MakeRotateMatrixZ(float radian);

Matrix4x4 MakeRotateMatrixXYZ(Vector3 rad);

// 11. 3次元のアフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

// 12. レンタリングパイプラインVer2

//  透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float FovY, float aspectRatio, float nearClip, float farClip);
//  正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

Matrix4x4 MakeOrthographicMatrix_s(float left, float top, float right, float bottom, float nearClip, float farClip);
//  ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

// Vector3からスケール行列を作成する関数
Matrix4x4 ScaleMatrixFromVector3(const Vector3& scale);

// Vector3から平行移動行列を作成する関数
Matrix4x4 TranslationMatrixFromVector3(const Vector3& translate);

// 行列から平行移動成分だけ取り出す関数
Vector3 ExtractTranslation(const Matrix4x4& matrix);

// 行列から回転成分をオイラー角に変換する関数
Vector3 MatrixToEuler(const Matrix4x4& m);

DirectX::XMMATRIX ConvertToXMMATRIX(const Matrix4x4& matrix);

Matrix4x4 MatrixLookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up);