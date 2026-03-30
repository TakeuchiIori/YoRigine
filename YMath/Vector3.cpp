#include "Vector3.h"
#include <MathFunc.h>


// Vector3 : 加算
Vector3 Add(const Vector3& v1, const Vector3& v2) {
    Vector3 result;
    result.x = v1.x + v2.x;
    result.y = v1.y + v2.y;
    result.z = v1.z + v2.z;
    return result;
}

// Vector3 : 減算
Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
    Vector3 result;
    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    result.z = v1.z - v2.z;
    return result;
}

Vector3 Cross(const Vector3& v1, const Vector3& v2)
{
    Vector3 result;
    result.x = v1.y * v2.z - v1.z * v2.y;
    result.y = v1.z * v2.x - v1.x * v2.z;
    result.z = v1.x * v2.y - v1.y * v2.x;
    return result;
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
{
    return a * (1.0f - t) + b * t;
}

Vector3 Multiply(const Vector3& v, float scalar) {
    return { v.x * scalar, v.y * scalar, v.z * scalar };
}

Vector3 Normalize(const Vector3& vec) {
    // 実装例：ベクトルを正規化する関数
    float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    if (length > 0) {
        return Vector3{ vec.x / length, vec.y / length, vec.z / length };
    }
    return Vector3{ 0, 0, 0 };
}


Vector3 Normalize(Vector3& vec) {
    // 実装例：ベクトルを正規化する関数
    float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    if (length > 0) {
        return Vector3{ vec.x / length, vec.y / length, vec.z / length };
    }
    return Vector3{ 0, 0, 0 };
}

Vector3 Clamp(const Vector3& v, const Vector3& min, const Vector3& max)
{
    return Vector3(
        std::clamp(v.x, min.x, max.x),
        std::clamp(v.y, min.y, max.y),
        std::clamp(v.z, min.z, max.z)
    );
}

Vector3 CatmullRomInterpolation(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    const float s = 0.5f;

    float t2 = t * t;
    float t3 = t2 * t;

    Vector3 e3 = -1 * p0 + 3 * p1 - 3 * p2 + p3;
    Vector3 e2 = 2 * p0 - 5 * p1 + 4 * p2 - p3;
    Vector3 e1 = -1 * p0 + p2;
    Vector3 e0 = 2 * p1;


    return s * (e3 * t3 + e2 * t2 + e1 * t + e0);
};


Vector3 lerp(const Vector3& a, const Vector3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// 2つのベクトルを球面線形補間 (方向のみ)
inline Vector3 SlerpDirection(const Vector3& v0, const Vector3& v1, float t)
{
    // 正規化
    Vector3 n0 = Normalize(v0);
    Vector3 n1 = Normalize(v1);

    // 端ケース: ゼロベクトルなら、そのまま返す (または他の扱い)
    float len0 = Length(v0);
    float len1 = Length(v1);
    if (len0 < 1e-8f && len1 < 1e-8f) {
        return { 0.0f, 0.0f, 0.0f }; // 両方ゼロなら途中もゼロ
    }
    else if (len0 < 1e-8f) {
        return v1; // 片方がゼロならもう一方を返すなど
    }
    else if (len1 < 1e-8f) {
        return v0;
    }

    // ドットを計算 (クランプしてacosの範囲に収める)
    float dotVal = Dot(n0, n1);
    dotVal = std::max(-1.0f, std::min(dotVal, 1.0f));

    // 角度
    float omega = std::acos(dotVal);

    // ごく小さい角度なら、ほぼ同じ方向なので線形でも十分
    if (std::fabs(omega) < 1e-5f) {
        // 単純に線形補間でも可
        // return Normalize( (1.0f - t)*n0 + t*n1 );
        // あるいは n0 を返してもほぼ同じ
        return n0; // ほとんど同じ方向なら n0 のままでもOK
    }

    // sin(omega) で割るために確保
    float sinOmega = std::sin(omega);
    if (std::fabs(sinOmega) < 1e-5f) {
        // ほぼ 0 → 角度が180°近いかもしれない
        // 反平行に近い場合は「垂直なベクトル」を探して補間
        // 簡易的には線形で 0.5*(n0 + n1) などしてお茶を濁してもよい
        // 下記は簡単実装例
        Vector3 mid = { n0.y, -n0.x, 0.0f }; // n0に垂直な適当なベクトル
        // midがほぼゼロかもしれないので再度Normalize
        mid = Normalize(mid);
        // n0 と mid をかけ合わせるなど色々やり方あり
        return mid;
    }

    // Slerp の公式 (方向のみ)
    float oneMinusT = 1.0f - t;
    float sinOneMinusT = std::sin(oneMinusT * omega);
    float sinT = std::sin(t * omega);

    float scale0 = sinOneMinusT / sinOmega; // (sin((1-t)*omega) / sin(omega))
    float scale1 = sinT / sinOmega;         // (sin(t*omega) / sin(omega))

    // 補間方向ベクトル (単位ベクトル)
    Vector3 dir = {
        scale0 * n0.x + scale1 * n1.x,
        scale0 * n0.y + scale1 * n1.y,
        scale0 * n0.z + scale1 * n1.z,
    };
    return dir; // これは単位ベクトルに近いはず
}

// 方向はSlerp、長さは線形補間する例
// v(t) = length(t)*SlerpDirection(v0, v1, t)
Vector3 Slerp(const Vector3& v0, const Vector3& v1, float t)
{
    // 長さを線形補間
    float len0 = Length(v0);
    float len1 = Length(v1);
    float lenT = (1.0f - t) * len0 + t * len1; // 線形補間

    // 方向をSlerp
    Vector3 dir = SlerpDirection(v0, v1, t);

    // 最終的に "方向 * 補間した長さ"
    return { dir.x * lenT, dir.y * lenT, dir.z * lenT };
}

// Blender → DirectX変換（Position）
Vector3 ConvertPosition(const Vector3& pos) {
    return { pos.x, pos.z, pos.y };
}


// 2つのベクトルから、fromからtoへの回転を求める関数
Vector3 GetEulerAnglesFromToDirection(const Vector3& from, const Vector3& to)
{
    Vector3 dir = Normalize(to - from);

    // Yaw（Y軸回転）とPitch（X軸回転）を使う
    float yaw = std::atan2(dir.x, dir.z);                  // 左右
    float pitch = std::asin(-dir.y);                       // 上下

    return Vector3(pitch, yaw, 0.0f); // Z軸回転（Roll）は0でOK
}