#include "Quaternion.h"
#include "MathFunc.h"

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate)
{
    Matrix4x4 result;

    Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

    result = scaleMatrix * rotateMatrix * translateMatrix;
    return result;
}





Quaternion IdentityQuaternion()
{
    return { 0, 0, 0, 1 };
}

Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    };
}

Quaternion Conjugate(const Quaternion& quaternion) {
    return { -quaternion.x, -quaternion.y, -quaternion.z, quaternion.w };
}

float Norm(const Quaternion& quaternion) {
    return std::sqrt(quaternion.x * quaternion.x +
        quaternion.y * quaternion.y +
        quaternion.z * quaternion.z +
        quaternion.w * quaternion.w);
}

Quaternion Normalize(const Quaternion& quaternion) {
    float norm = Norm(quaternion);
    if (norm == 0.0f) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }
    return { quaternion.x / norm, quaternion.y / norm, quaternion.z / norm, quaternion.w / norm };
}

Quaternion Inverse(const Quaternion& quaternion) {
    Quaternion conjugate = Conjugate(quaternion);
    float norm = Norm(quaternion);
    float normSq = norm * norm;
    if (normSq == 0.0f) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }
    return { conjugate.x / normSq, conjugate.y / normSq, conjugate.z / normSq, conjugate.w / normSq };
}

Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle) {
    Vector3 normAxis = Normalize(axis);
    float sinHalfAngle = std::sin(angle / 2.0f);
    float cosHalfAngle = std::cos(angle / 2.0f);
    return { normAxis.x * sinHalfAngle, normAxis.y * sinHalfAngle, normAxis.z * sinHalfAngle, cosHalfAngle };
}

//Quaternion CombineRotations(const Quaternion& q1, const Quaternion& q2)
//{
//    return {
//        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
//        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
//        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
//        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
//    };
//}

/// <summary>
/// 2つのクォータニオンをハミルトン積（乗算）で合成する関数
/// </summary>
Quaternion CombineRotations(const Quaternion& lhs, const Quaternion& rhs)
{
    // ハミルトン積の計算（左：lhs, 右：rhs）
    // w成分
    float newW = lhs.w * rhs.w
        - lhs.x * rhs.x
        - lhs.y * rhs.y
        - lhs.z * rhs.z;

    // x成分
    float newX = lhs.w * rhs.x
        + lhs.x * rhs.w
        + lhs.y * rhs.z
        - lhs.z * rhs.y;

    // y成分
    float newY = lhs.w * rhs.y
        - lhs.x * rhs.z
        + lhs.y * rhs.w
        + lhs.z * rhs.x;

    // z成分
    float newZ = lhs.w * rhs.z
        + lhs.x * rhs.y
        - lhs.y * rhs.x
        + lhs.z * rhs.w;

    return { newX, newY, newZ, newW };
}


Quaternion MakeRotateAxisAngleQuaternion(const Vector3& angles) {
    // 度→ラジアン
    const float deg2Rad = 3.14159265359f / 180.0f;
    float radX = angles.x * deg2Rad;
    float radY = angles.y * deg2Rad;
    float radZ = angles.z * deg2Rad;

    // X軸回転
    float sinHalfX = sin(radX * 0.5f);
    float cosHalfX = cos(radX * 0.5f);
    // (x, y, z, w)
    Quaternion quatX = {
        sinHalfX,  // x
        0.0f,      // y
        0.0f,      // z
        cosHalfX   // w
    };

    // Y軸回転
    float sinHalfY = sin(radY * 0.5f);
    float cosHalfY = cos(radY * 0.5f);
    Quaternion quatY = {
        0.0f,
        sinHalfY,
        0.0f,
        cosHalfY
    };

    // Z軸回転
    float sinHalfZ = sin(radZ * 0.5f);
    float cosHalfZ = cos(radZ * 0.5f);
    Quaternion quatZ = {
        0.0f,
        0.0f,
        sinHalfZ,
        cosHalfZ
    };

    // 順序：X → Y → Z に回したいなら → Q = Z * Y * X
    // 実際に適用されるのは「X軸回転 → Y軸回転 → Z軸回転」
    Quaternion zy = CombineRotations(quatZ, quatY);
    Quaternion zyx = CombineRotations(zy, quatX);

    return zyx;
}


Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion) {
    Quaternion qVector = { vector.x, vector.y, vector.z, 0.0f };
    Quaternion qConjugate = { -quaternion.x, -quaternion.y, -quaternion.z, quaternion.w };
    Quaternion qResult = Multiply(Multiply(quaternion, qVector), qConjugate);
    return { qResult.x, qResult.y, qResult.z };
}

Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion) {
    Matrix4x4 matrix = {};

    float xx = quaternion.x * quaternion.x;
    float yy = quaternion.y * quaternion.y;
    float zz = quaternion.z * quaternion.z;
    float xy = quaternion.x * quaternion.y;
    float xz = quaternion.x * quaternion.z;
    float yz = quaternion.y * quaternion.z;
    float wx = quaternion.w * quaternion.x;
    float wy = quaternion.w * quaternion.y;
    float wz = quaternion.w * quaternion.z;

    matrix.m[0][0] = 1.0f - 2.0f * (yy + zz);
    matrix.m[0][1] = 2.0f * (xy - wz);
    matrix.m[0][2] = 2.0f * (xz + wy);
    matrix.m[0][3] = 0.0f;

    matrix.m[1][0] = 2.0f * (xy + wz);
    matrix.m[1][1] = 1.0f - 2.0f * (xx + zz);
    matrix.m[1][2] = 2.0f * (yz - wx);
    matrix.m[1][3] = 0.0f;

    matrix.m[2][0] = 2.0f * (xz - wy);
    matrix.m[2][1] = 2.0f * (yz + wx);
    matrix.m[2][2] = 1.0f - 2.0f * (xx + yy);
    matrix.m[2][3] = 0.0f;

    matrix.m[3][0] = 0.0f;
    matrix.m[3][1] = 0.0f;
    matrix.m[3][2] = 0.0f;
    matrix.m[3][3] = 1.0f;

    matrix = TransPose(matrix);

    return matrix;
}

float Dot(const Quaternion& q0, const Quaternion& q1) {
    return q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
}

Quaternion Lerp(const Quaternion& q1, const Quaternion& q2, float t)
{
    // t が範囲外の場合はクランプする
    float tt = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);

    // 線形補間 (1 - t)*q1 + t*q2
    Quaternion result;
    result.x = (1.0f - tt) * q1.x + tt * q2.x;
    result.y = (1.0f - tt) * q1.y + tt * q2.y;
    result.z = (1.0f - tt) * q1.z + tt * q2.z;
    result.w = (1.0f - tt) * q1.w + tt * q2.w;

    // 結果を正規化
    float len = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
    if (len > 0.0f)
    {
        float invLen = 1.0f / len;
        result.x *= invLen;
        result.y *= invLen;
        result.z *= invLen;
        result.w *= invLen;
    }

    return result;
}

Quaternion Slerp(Quaternion q0, Quaternion q1, float t) {
    float dot = Dot(q0, q1);
    if (dot < 0.0f) {
        q0 = { -q0.x, -q0.y, -q0.z, -q0.w }; // 反対方向に補間
        dot = -dot;
    }

    // なす角を求める
    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);

    // 補間係数を求める
    if (sinTheta > 0.001f) { // 数値安定性のための閾値
        float scale0 = std::sin((1 - t) * theta) / sinTheta;
        float scale1 = std::sin(t * theta) / sinTheta;

        // 補間後のQuaternionを計算
        return q0 * scale0 + q1 * scale1;
    }
    else {
        // ほぼ同じ方向の場合、線形補間
        return q0 * (1 - t) + q1 * t;
    }
}

Quaternion CubicSplineInterpolate(const Quaternion& q0, const Quaternion& t0, const Quaternion& q1, const Quaternion& t1, float t) {
    // t の 2乗と 3乗を計算
    float t2 = t * t;
    float t3 = t2 * t;

    // 補間係数を計算
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;

    // スプライン補間を計算
    return (q0 * h00) + (t0 * h10) + (q1 * h01) + (t1 * h11);
}

Quaternion CubicSplineQuaternionInterpolation(const std::vector<float>& keyTimes, const std::vector<Quaternion>& keyValues, const std::vector<Quaternion>& keyInTangents, const std::vector<Quaternion>& keyOutTangents, float time)
{
    assert(keyTimes.size() == keyValues.size());
    assert(keyValues.size() == keyInTangents.size());
    assert(keyInTangents.size() == keyOutTangents.size());
    assert(!keyTimes.empty());

    // 時間が範囲外の場合、最初または最後の値を返す
    if (time <= keyTimes.front()) {
        return keyValues.front();
    }
    if (time >= keyTimes.back()) {
        return keyValues.back();
    }

    // 補間に使用する区間を探索
    size_t segmentIndex = 0;
    for (size_t i = 0; i < keyTimes.size() - 1; ++i) {
        if (time >= keyTimes[i] && time <= keyTimes[i + 1]) {
            segmentIndex = i;
            break;
        }
    }

    // セグメントデータを取得
    float t0 = keyTimes[segmentIndex];
    float t1 = keyTimes[segmentIndex + 1];
    const Quaternion& p0 = keyValues[segmentIndex];
    const Quaternion& p1 = keyValues[segmentIndex + 1];
    const Quaternion& m0 = keyOutTangents[segmentIndex];
    const Quaternion& m1 = keyInTangents[segmentIndex + 1];

    // 補間パラメータ t を計算
    float t = (time - t0) / (t1 - t0);

    // Hermiteスプライン補間式を適用
    float t2 = t * t;
    float t3 = t2 * t;

    Quaternion h00 = (2 * t3 - 3 * t2 + 1) * p0;
    Quaternion h10 = (t3 - 2 * t2 + t) * m0;
    Quaternion h01 = (-2 * t3 + 3 * t2) * p1;
    Quaternion h11 = (t3 - t2) * m1;

    return h00 + h10 + h01 + h11;
}


std::vector<Quaternion> CubicSplineQuaternionInterpolation(
    const std::vector<float>& xData,
    const std::vector<Quaternion>& yData,
    const std::vector<Quaternion>& inTangents,
    const std::vector<Quaternion>& outTangents,
    const std::vector<float>& xQuery
) {
    // xData, yData, inTangents, outTangents が妥当かチェック
    if (xData.size() != yData.size() || xData.size() != inTangents.size() || xData.size() != outTangents.size()) {
        throw std::invalid_argument("xData, yData, inTangents, outTangents のサイズが一致していません。");
    }
    if (xData.size() < 2) {
        throw std::invalid_argument("データ点が不足しています。");
    }

    // 入力データが単調増加になっているか確認
    for (size_t i = 1; i < xData.size(); ++i) {
        if (xData[i] <= xData[i - 1]) {
            throw std::invalid_argument("xData は単調増加である必要があります。");
        }
    }

   // const size_t n = xData.size();
    std::vector<Quaternion> result;
    result.reserve(xQuery.size());

    for (float x : xQuery) {
        // x が xData の範囲外にある場合の扱い（今回は外挿せずに端点近辺を使う）
        if (x <= xData.front()) {
            result.push_back(yData.front());
            continue;
        }
        else if (x >= xData.back()) {
            result.push_back(yData.back());
            continue;
        }

        // x が属する区間を探す
        auto it = std::upper_bound(xData.begin(), xData.end(), x);
        size_t i = static_cast<size_t>(std::distance(xData.begin(), it) - 1);

        // i番目の区間 [xData[i], xData[i+1]) に x は属する
        float t = (x - xData[i]) / (xData[i + 1] - xData[i]); // 正規化された時間

        // Cubic Spline 補間を計算
        Quaternion h00 = yData[i] * (2 * t * t * t - 3 * t * t + 1);
        Quaternion h10 = outTangents[i] * (t * t * t - 2 * t * t + t);
        Quaternion h01 = yData[i + 1] * (-2 * t * t * t + 3 * t * t);
        Quaternion h11 = inTangents[i + 1] * (t * t * t - t * t);

        Quaternion interpolated = h00 + h10 + h01 + h11;
        result.push_back(interpolated);
    }

    return result;
}


// クォータニオンからオイラー角を作成する関数
Vector3 QuaternionToEuler(const Quaternion& q)
{
    Vector3 euler;

    // Roll (X軸回転)
    float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    euler.x = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (Y軸回転)
    float sinp = 2 * (q.w * q.y - q.z * q.x);
    if (std::abs(sinp) >= 1)
        euler.y = static_cast<float>(std::copysign(std::numbers::pi / 2, sinp)); // Gimbal lock
    else
        euler.y = std::asin(sinp);

    // Yaw (Z軸回転)
    float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    euler.z = std::atan2(siny_cosp, cosy_cosp);

    return euler;
}

Quaternion MakeAlignQuaternion(const Vector3& from, const Vector3& to) {
    Vector3 cross = Cross(from, to);
    float dot = Dot(from, to);

    if (dot >= 1.0f) {
        return Quaternion(0, 0, 0, 1); // 同じ方向の場合、回転なし
    }
    else if (dot <= -1.0f) {
        // 反対方向の場合、回転軸を選択する必要がある
        Vector3 axis = (std::abs(from.x) < std::abs(from.y)) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
        axis = Normalize(Cross(from, axis));
        return Quaternion(axis.x, axis.y, axis.z, 0); // 180度回転
    }

    float s = std::sqrt((1 + dot) * 2);
    float invs = 1 / s;

    return Quaternion(cross.x * invs, cross.y * invs, cross.z * invs, s * 0.5f);
}

/// <summary>
/// 2つのベクトルの間の回転を計算する関数
/// </summary>
Vector3 SetFromTo(const Vector3& from, const Vector3& to) {
    // ベクトルを正規化
    Vector3 normalizedFrom = Normalize(from);
    Vector3 normalizedTo = Normalize(to);

    // 内積を計算して、ベクトルの関係性を判断
    float dot = Dot(normalizedFrom, normalizedTo);
    Vector3 rotationAxis;
    float rotationAngle;

    // ベクトルが同じ方向の場合、回転は不要
    if (dot > 0.9999f) {
        return Vector3(0.0f, 0.0f, 0.0f);
    }
    // ベクトルが逆方向の場合、任意の垂直軸を使用して180度回転
    else if (dot < -0.9999f) {
        rotationAxis = Cross(Vector3(1.0f, 0.0f, 0.0f), normalizedFrom);
        if (Length(rotationAxis) < 0.0001f) {
            rotationAxis = Cross(Vector3(0.0f, 1.0f, 0.0f), normalizedFrom);
        }
        rotationAxis = Normalize(rotationAxis);
        rotationAngle = static_cast<float>(std::numbers::pi);
    }
    // それ以外の場合、通常の回転を計算
    else {
        rotationAxis = Normalize(Cross(normalizedFrom, normalizedTo));
        rotationAngle = acos(dot);
    }

    // オイラー角に変換（クォータニオン不要の場合）
    return rotationAxis * rotationAngle;
}

/// <summary>
/// 2つのベクトル間の回転を表すクォータニオンを生成する関数
/// </summary>
Quaternion SetFromToQuaternion(const Vector3& from, const Vector3& to) {

    Vector3 f = Normalize(from);
    Vector3 t = Normalize(to);

    Vector3 cross = Cross(f, t);

    float dot = Dot(f, t);
    float w = sqrt((1 + dot) * 0.5f);
    float s = 0.5f / w;


    float x = cross.x * s;
    float y = cross.y * s;
    float z = cross.z * s;

    return Quaternion{ x, y, z, w };
}

Vector3 RotateVectorByQuaternion(const Vector3& vec, const Quaternion& quat) {
    Quaternion qVec(0, vec.x, vec.y, vec.z);
    Quaternion result = quat * qVec * Conjugate(quat);
    return Vector3(result.x, result.y, result.z);
}

// オイラー角をクォータニオンに変換する関数
Quaternion EulerToQuaternion(const Vector3& euler)
{
    // オイラー角をラジアンに変換（もし入力が度であれば以下を有効化）
    constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
    float pitch = euler.x * DEG_TO_RAD; // x軸 (Pitch)
    float yaw = euler.y * DEG_TO_RAD; // y軸 (Yaw)
    float roll = euler.z * DEG_TO_RAD; // z軸 (Roll)

    // 半角を計算
    float cy = cos(yaw * 0.5f);
    float sy = sin(yaw * 0.5f);
    float cp = cos(pitch * 0.5f);
    float sp = sin(pitch * 0.5f);
    float cr = cos(roll * 0.5f);
    float sr = sin(roll * 0.5f);

    Quaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return q;
}
Quaternion MatrixToQuaternion(const Matrix4x4& mat) {
    Quaternion q;
    float trace = mat.m[0][0] + mat.m[1][1] + mat.m[2][2];

    if (trace > 0.0f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (mat.m[2][1] - mat.m[1][2]) * s;
        q.y = (mat.m[0][2] - mat.m[2][0]) * s;
        q.z = (mat.m[1][0] - mat.m[0][1]) * s;
    }
    else {
        if (mat.m[0][0] > mat.m[1][1] && mat.m[0][0] > mat.m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + mat.m[0][0] - mat.m[1][1] - mat.m[2][2]);
            q.w = (mat.m[2][1] - mat.m[1][2]) / s;
            q.x = 0.25f * s;
            q.y = (mat.m[0][1] + mat.m[1][0]) / s;
            q.z = (mat.m[0][2] + mat.m[2][0]) / s;
        }
        else if (mat.m[1][1] > mat.m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + mat.m[1][1] - mat.m[0][0] - mat.m[2][2]);
            q.w = (mat.m[0][2] - mat.m[2][0]) / s;
            q.x = (mat.m[0][1] + mat.m[1][0]) / s;
            q.y = 0.25f * s;
            q.z = (mat.m[1][2] + mat.m[2][1]) / s;
        }
        else {
            float s = 2.0f * sqrtf(1.0f + mat.m[2][2] - mat.m[0][0] - mat.m[1][1]);
            q.w = (mat.m[1][0] - mat.m[0][1]) / s;
            q.x = (mat.m[0][2] + mat.m[2][0]) / s;
            q.y = (mat.m[1][2] + mat.m[2][1]) / s;
            q.z = 0.25f * s;
        }
    }
    return q;
}

Quaternion LookAtQuaternion(const Vector3& from, const Vector3& to, const Vector3& up) {
    // from から to への方向ベクトルを計算
    Vector3 forward = Normalize(to - from);

    // right ベクトルを計算（up と forward の外積）
    Vector3 right = Normalize(Cross(up, forward));

    // 新しい up ベクトルを計算
    Vector3 newUp = Cross(forward, right);

    // LookAt 行列を設定
    Matrix4x4 lookAtMatrix;
    lookAtMatrix.m[0][0] = right.x;
    lookAtMatrix.m[0][1] = right.y;
    lookAtMatrix.m[0][2] = right.z;
    lookAtMatrix.m[0][3] = 0.0f;

    lookAtMatrix.m[1][0] = newUp.x;
    lookAtMatrix.m[1][1] = newUp.y;
    lookAtMatrix.m[1][2] = newUp.z;
    lookAtMatrix.m[1][3] = 0.0f;

    lookAtMatrix.m[2][0] = forward.x;
    lookAtMatrix.m[2][1] = forward.y;
    lookAtMatrix.m[2][2] = forward.z;
    lookAtMatrix.m[2][3] = 0.0f;

    lookAtMatrix.m[3][0] = 0.0f;
    lookAtMatrix.m[3][1] = 0.0f;
    lookAtMatrix.m[3][2] = 0.0f;
    lookAtMatrix.m[3][3] = 1.0f;

    // 行列をクォータニオンに変換
    return MatrixToQuaternion(lookAtMatrix);
}

Vector3 QuaternionToForward(const Quaternion& quat) {
    // クォータニオンから前方向ベクトルを計算
    // 通常、前方向はz軸を指すため、基準ベクトル(0, 0, 1)に回転を適用します
    float x = 2 * (quat.x * quat.z + quat.w * quat.y);
    float y = 2 * (quat.y * quat.z - quat.w * quat.x);
    float z = 1 - 2 * (quat.x * quat.x + quat.y * quat.y);
    return Vector3(x, y, z);
}