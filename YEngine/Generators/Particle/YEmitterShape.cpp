#include "YEmitterShape.h"
//=================================================================
// 形状ごとのランダム生成ロジック
//=================================================================

// 乱数生成器のヘルパー
static float RandomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

Vector3 YEmitterSphere::GeneratePoint()
{
    // 単位球内のランダム点
    // 簡易的な実装: 立方体内で生成して球外なら棄却、あるいは極座標変換
    float theta = RandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
    float phi = std::acos(RandomFloat(-1.0f, 1.0f));

    // 距離のランダム化（shellOnlyなら表面固定、そうでなければ体積内一様分布）
    float r = radius;
    if (!shellOnly) {
        // 体積一様に分布させるため、0~1の乱数の立方根をとる
        float u = RandomFloat(0.0f, 1.0f);
        r *= std::cbrt(u);
    }

    float sinPhi = std::sin(phi);
    return Vector3{
        r * sinPhi * std::cos(theta),
        r * sinPhi * std::sin(theta),
        r * std::cos(phi)
    };
}

Vector3 YEmitterBox::GeneratePoint()
{
    float x = RandomFloat(-size.x * 0.5f, size.x * 0.5f);
    float y = RandomFloat(-size.y * 0.5f, size.y * 0.5f);
    float z = RandomFloat(-size.z * 0.5f, size.z * 0.5f);
    return Vector3{ x, y, z };
}
