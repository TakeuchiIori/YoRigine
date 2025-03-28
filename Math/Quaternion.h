#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

struct Quaternion
{
    float x, y, z, w;

    // 単位クォータニオンを簡単に返すための静的メンバ関数
    static Quaternion Identity() {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    // クォータニオンの逆（共役）を返すメソッド
    Quaternion Inverse() const {
        float lengthSq = x * x + y * y + z * z + w * w;
        return Quaternion(-x / lengthSq, -y / lengthSq, -z / lengthSq, w / lengthSq);
    }

    // クォータニオンの掛け算（演算子オーバーロード）
    Quaternion operator*(const Quaternion& q) const {
        return Quaternion(
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        );
    }

    // 加算
    Quaternion operator+(const Quaternion& other) const {
        return Quaternion(w + other.w, x + other.x, y + other.y, z + other.z);
    }

    // 減算
    Quaternion operator-(const Quaternion& other) const {
        return Quaternion(w - other.w, x - other.x, y - other.y, z - other.z);
    }

    // 加算の代入演算子
    Quaternion& operator+=(const Quaternion& other) {
        w += other.w;
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    // 減算の代入演算子
    Quaternion& operator-=(const Quaternion& other) {
        w -= other.w;
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    // スカラー倍（右辺）
    Quaternion operator*(float scalar) const {
        return Quaternion(w * scalar, x * scalar, y * scalar, z * scalar);
    }

    // スカラー倍の代入演算子
    Quaternion& operator*=(float scalar) {
        w *= scalar;
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    // スカラー除算
    Quaternion operator/(float scalar) const {
        return Quaternion(w / scalar, x / scalar, y / scalar, z / scalar);
    }

    // スカラー乗算（左辺）を友達関数として定義
    friend Quaternion operator*(float scalar, const Quaternion& q) {
        return Quaternion(q.w * scalar, q.x * scalar, q.y * scalar, q.z * scalar);
       
    }

    Vector3 RotateVector(const Vector3& v) const {
        Quaternion q = *this;
        Quaternion qInv = q.Inverse();
        Quaternion vectorQuat(0, v.x, v.y, v.z);
        Quaternion rotatedQuat = q * vectorQuat * qInv;
        return Vector3(rotatedQuat.x, rotatedQuat.y, rotatedQuat.z);
    }

    // ベクトルとクォータニオンの乗算演算子
    friend Vector3 operator*(const Quaternion& q, const Vector3& v) {
        return q.RotateVector(v);
    }
};

struct QuaternionTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};


// Quaternion回転を含むアフィン変換
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

// 単位クォータニオンを返す関数
Quaternion IdentityQuaternion();

// 2つのクォータニオンの積を計算する関数
Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);

// クォータニオンの共役を返す関数
Quaternion Conjugate(const Quaternion& quaternion);

// クォータニオンのノルム（大きさ）を計算する関数
float Norm(const Quaternion& quaternion);

// クォータニオンを正規化する関数
Quaternion Normalize(const Quaternion& quaternion);

// クォータニオンの逆を計算する関数
Quaternion Inverse(const Quaternion& quaternion);

// 指定した軸と角度で回転を表すクォータニオンを作成する関数
Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle);

// 複数の回転を合成
Quaternion CombineRotations(const Quaternion& q1, const Quaternion& q2);

Quaternion MakeRotateAxisAngleQuaternion(const Vector3& angles);

Quaternion CombineRotations(const Quaternion& lhs, const Quaternion& rhs);

// クォータニオンを使ってベクトルを回転させる関数
Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion);

// クォータニオンから回転行列を作成する関数
Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);

// 2つのクォータニオンの内積を計算する関数
float Dot(const Quaternion& q0, const Quaternion& q1);

Quaternion Lerp(const Quaternion& q1, const Quaternion& q2, float t);

// 2つのクォータニオン間で球面線形補間（Slerp）を行う関数
Quaternion Slerp(Quaternion q0, Quaternion q1, float t);

// 4つのクォータニオン間で球面線形補間（Slerp）を行う関数
Quaternion CubicSplineInterpolate(const Quaternion& q0, const Quaternion& t0, const Quaternion& q1, const Quaternion& t1, float t);

Quaternion CubicSplineQuaternionInterpolation(
	const std::vector<float>& keyTimes,
	const std::vector<Quaternion>& keyValues,
	const std::vector<Quaternion>& keyInTangents,
	const std::vector<Quaternion>& keyOutTangents,
	float time
);

// クォータニオンからオイラー角を作成する関数
Vector3 QuaternionToEuler(const Quaternion& q);

// 2つの方向ベクトルを揃えるクォータニオンを計算
Quaternion MakeAlignQuaternion(const Vector3& from, const Vector3& to);

Vector3 SetFromTo(const Vector3& from, const Vector3& to);

Quaternion SetFromToQuaternion(const Vector3& from, const Vector3& to);

Vector3 RotateVectorByQuaternion(const Vector3& vec, const Quaternion& quat);

Quaternion EulerToQuaternion(const Vector3& euler);

Quaternion MatrixToQuaternion(const Matrix4x4& mat);

Quaternion LookAtQuaternion(const Vector3& from, const Vector3& to, const Vector3& up);

// Quaternionから前方向ベクトルを取得する関数
Vector3 QuaternionToForward(const Quaternion& quat);
