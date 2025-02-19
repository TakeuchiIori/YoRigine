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

Vector3 CubicSplineInterpolate(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    // t の二乗および三乗を計算
    float t2 = t * t;
    float t3 = t2 * t;

    // Catmull-Rom スプラインの基底行列
    Vector3 a = p1 * 2.0f;
    Vector3 b = (p2 - p0) * t;
    Vector3 c = (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2;
    Vector3 d = (-1 * p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3;

    Vector3 result = (a + b + c + d) * 0.5f;
    return result;
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
std::vector<double> CubicSplineInterpolation(
    const std::vector<double>& xData,
    const std::vector<double>& yData,
    const std::vector<double>& xQuery
)
{
    // xData, yData が妥当かチェック
    if (xData.size() != yData.size() || xData.size() < 2) {
        throw std::invalid_argument("xData と yData のサイズ不一致、またはデータ点が不足しています。");
    }

    // 入力データが単調増加になっているか（最低限の確認）
    for (size_t i = 1; i < xData.size(); ++i) {
        if (xData[i] <= xData[i - 1]) {
            throw std::invalid_argument("xData は単調増加である必要があります。");
        }
    }

    // データ点の数
    const size_t n = xData.size();

    // 便宜上 yData を a にコピー
    // スプライン係数の計算で a[i] が f_i (yData[i]) を表すことが多いため
    std::vector<double> a = yData;

    // 区間幅 h[i] = xData[i+1] - xData[i] (i=0..n-2)
    std::vector<double> h(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        h[i] = xData[i + 1] - xData[i];
    }

    // 求めたいスプライン係数: b, c, d
    // 今回は自然スプラインなので、両端条件: c[0] = 0, c[n-1] = 0
    std::vector<double> b(n), c(n), d(n);

    // c を求めるための三重対角行列を解く
    // まず alpha[i] を計算
    std::vector<double> alpha(n);
    alpha[0] = 0.0; // 自然スプラインの場合、両端は 0
    alpha[n - 1] = 0.0;

    for (size_t i = 1; i < n - 1; ++i) {
        alpha[i] = (3.0 / h[i]) * (a[i + 1] - a[i])
            - (3.0 / h[i - 1]) * (a[i] - a[i - 1]);
    }

    // l, mu, z は三重対角行列を前進・後退代入で解く際に使用
    std::vector<double> l(n), mu(n), z(n);
    l[0] = 1.0;
    mu[0] = 0.0;
    z[0] = 0.0;

    for (size_t i = 1; i < n - 1; ++i) {
        l[i] = 2.0 * (xData[i + 1] - xData[i - 1]) - h[i - 1] * mu[i - 1];
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    l[n - 1] = 1.0;
    z[n - 1] = 0.0;
    c[n - 1] = 0.0;

    // 後退代入 (c を求める)
    for (int j = static_cast<int>(n) - 2; j >= 0; --j) {
        c[j] = z[j] - mu[j] * c[j + 1];
    }

    // b, d を求める
    for (size_t i = 0; i < n - 1; ++i) {
        b[i] = (a[i + 1] - a[i]) / h[i] - (h[i] / 3.0) * (c[i + 1] + 2.0 * c[i]);
        d[i] = (c[i + 1] - c[i]) / (3.0 * h[i]);
    }
    // b[n-1], d[n-1] は使わないが、エラーを避けるため初期化
    b[n - 1] = b[n - 2];
    d[n - 1] = d[n - 2];

    // xQuery に対して補間結果を求める
    std::vector<double> result;
    result.reserve(xQuery.size());

    for (auto x : xQuery) {
        // x が xData の範囲外にある場合の扱い (今回は外挿せずに端点近辺を使う例)
        if (x <= xData.front()) {
            // 区間[0,1]の延長 (厳密には外挿)
            double dx = x - xData[0];
            double y = a[0]
                + b[0] * dx
                + c[0] * dx * dx
                + d[0] * dx * dx * dx;
            result.push_back(y);
            continue;
        }
        else if (x >= xData.back()) {
            // 区間[n-2, n-1]の延長
            size_t i = n - 2;
            double dx = x - xData[i];
            double y = a[i]
                + b[i] * dx
                + c[i] * dx * dx
                + d[i] * dx * dx * dx;
            result.push_back(y);
            continue;
        }

        // 2分探索などで x の属する区間を探す
        // std::upper_bound を使って x が挿入される位置を探す
        auto it = std::upper_bound(xData.begin(), xData.end(), x);
        size_t i = static_cast<size_t>(std::distance(xData.begin(), it) - 1);

        // i番目の区間 [xData[i], xData[i+1]) に x は属するとする
        double dx = x - xData[i];
        double y = a[i]
            + b[i] * dx
            + c[i] * dx * dx
            + d[i] * dx * dx * dx;
        result.push_back(y);
    }

    return result;
}

Vector3 CatmullRomSpline(const std::vector<Vector3>& controlPoints, float t) {
    // コントロールポイントが4つでないときはエラーを表示する。
    if (controlPoints.size() != 4) {
        throw std::invalid_argument("Catmull-Rom Splineには4つのコントロールポイントが必要です。");
    }

    // t の2乗と3乗を計算
    float t2 = t * t;
    float t3 = t2 * t;

    // 各コントロールポイントを変数に代入
    const Vector3& p0 = controlPoints[0];
    const Vector3& p1 = controlPoints[1];
    const Vector3& p2 = controlPoints[2];
    const Vector3& p3 = controlPoints[3];

    // X, Y, Z 軸成分の Catmull-Rom 補間計算
    float x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    float y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    float z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);

    // 計算された位置をVector3で返す
    return Vector3(x, y, z);
}

std::vector<Vector3> GenerateCatmullRomSplinePoints(const std::vector<Vector3>& controlPoints, size_t segmentCount) {
    std::vector<Vector3> pointsDrawing;

    // コントロールポイント数の確認
    if (controlPoints.size() < 4) {
        throw std::invalid_argument("Catmull-Rom Splineには少なくとも4つのコントロールポイントが必要です。");
    }

    // 各4つのコントロールポイントについて補間を行う
    for (size_t i = 0; i < controlPoints.size() - 3; ++i) {
        std::vector<Vector3> segmentControlPoints = {
            controlPoints[i],
            controlPoints[i + 1],
            controlPoints[i + 2],
            controlPoints[i + 3]
        };

        // セグメントを分割して頂点を生成
        for (size_t j = 0; j <= segmentCount; ++j) {
            float t = static_cast<float>(j) / static_cast<float>(segmentCount);
            Vector3 pos = CatmullRomSpline(segmentControlPoints, t);
            pointsDrawing.push_back(pos);
        }
    }

    return pointsDrawing;
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


Vector3 CatmullRomPosition(const std::vector<Vector3>& points, float t) {
    assert(points.size() >= 4 && "制御点は4点以上必要です");

    // 区間数は制御点の数-1
    size_t division = points.size() - 1;
    // 1区間の長さ
    float areaWidth = 1.0f / division;

    // 区間内の始点を0.0f, 終点を1.0fとした時の現在位置
    float t_2 = std::fmod(t, areaWidth) * division;
    // 加減(0.0f)と上限(1.0f)の範囲に収める
    t_2 = std::clamp(t_2, 0.0f, 1.0f);

    // 区間番号を計算し、範囲内に収める
    size_t index = static_cast<size_t>(t / areaWidth);
    // 上限を超えないように抑える
    index = std::min(index, points.size() - 2);

    // 4点分のインデックス
    size_t index0 = index - 1;
    size_t index1 = index;
    size_t index2 = index + 1;
    size_t index3 = index + 2;

    // 最初の区間のp0はp1を重複使用する
    if (index == 0) {
        index0 = index1;
    }
    // 最後の区間のp3はp2を重複使用する
    if (index3 >= points.size()) {
        index3 = index2;
    }

    // 4点の座標
    const Vector3& p0 = points[index0];
    const Vector3& p1 = points[index1];
    const Vector3& p2 = points[index2];
    const Vector3& p3 = points[index3];

    return CatmullRomInterpolation(p0, p1, p2, p3, t_2);
}

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