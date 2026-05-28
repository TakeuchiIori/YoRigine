#include "YEmitterShape.h"

// ────────────────────────────────────────────────────────────────
// 内部乱数ヘルパー
// ────────────────────────────────────────────────────────────────
static float Rnd(float minV, float maxV)
{
    static std::mt19937 gen{ std::random_device{}() };
    std::uniform_real_distribution<float> d(minV, maxV);
    return d(gen);
}

static constexpr float kPi = 3.14159265f;

// ────────────────────────────────────────────────────────────────
// YEmitterSphere
// ────────────────────────────────────────────────────────────────

Vector3 YEmitterSphere::GeneratePoint()
{
    // 球面上の一様分布方向
    float theta = Rnd(0.0f, 2.0f * kPi);
    float phi   = std::acos(Rnd(-1.0f, 1.0f));

    // 半径を minRadius〜maxRadius でランダムに選択
    // 体積一様にするため累乗根を使う（minRadius > 0 の空洞も正しく扱う）
    float rMin3 = minRadius * minRadius * minRadius;
    float rMax3 = maxRadius * maxRadius * maxRadius;
    float r = std::cbrt(Rnd(rMin3, rMax3));

    float sinPhi = std::sin(phi);
    return {
        r * sinPhi * std::cos(theta),
        r * sinPhi * std::sin(theta),
        r * std::cos(phi)
    };
}

void YEmitterSphere::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("内側半径 (min)", &minRadius, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("外側半径 (max)", &maxRadius, 0.01f, 0.0f, 100.0f);
    // minRadius > maxRadius を防ぐ
    if (minRadius > maxRadius) minRadius = maxRadius;

    shellOnly = (minRadius >= maxRadius);
    if (shellOnly) {
        ImGui::TextColored({0.8f,0.8f,0.2f,1}, "min = max → 球殻モード");
    } else {
        ImGui::TextDisabled("min < max → 球体ボリュームモード");
    }
#endif
}

// ────────────────────────────────────────────────────────────────
// YEmitterBox
// ────────────────────────────────────────────────────────────────

Vector3 YEmitterBox::GeneratePoint()
{
    // 各軸で minSize〜maxSize のランダムサイズを決めてから内側をくり抜く
    // くり抜き実装: 各軸の符号をランダムに決め、内外半径で位置を決定
    float x = Rnd(minSize.x, maxSize.x);
    float y = Rnd(minSize.y, maxSize.y);
    float z = Rnd(minSize.z, maxSize.z);

    // 正負をランダムに
    x *= (Rnd(0,1) < 0.5f) ? -1.0f : 1.0f;
    y *= (Rnd(0,1) < 0.5f) ? -1.0f : 1.0f;
    z *= (Rnd(0,1) < 0.5f) ? -1.0f : 1.0f;

    return { x, y, z };
}

void YEmitterBox::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("内側サイズ (min)", &minSize.x, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat3("外側サイズ (max)", &maxSize.x, 0.01f, 0.0f, 100.0f);
    // 各軸で min > max を防ぐ
    if (minSize.x > maxSize.x) minSize.x = maxSize.x;
    if (minSize.y > maxSize.y) minSize.y = maxSize.y;
    if (minSize.z > maxSize.z) minSize.z = maxSize.z;

    bool isHollow = (minSize.x > 0 || minSize.y > 0 || minSize.z > 0);
    if (isHollow) ImGui::TextColored({0.8f,0.8f,0.2f,1}, "中空ボックスモード");
    else          ImGui::TextDisabled("中実ボックスモード");
#endif
}

// ────────────────────────────────────────────────────────────────
// YEmitterCone
// ────────────────────────────────────────────────────────────────

Vector3 YEmitterCone::GeneratePoint()
{
    // コーン内ランダム方向
    float innerRad = innerAngle * kPi / 180.0f;
    float outerRad = outerAngle * kPi / 180.0f;

    // コサイン範囲でランダム（theta を均一に分布させるため）
    float cosInner = std::cos(outerRad);  // 外側は広いほどコサインが小さい
    float cosOuter = std::cos(innerRad);
    float cosTheta = Rnd(cosInner, cosOuter);
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = Rnd(0.0f, 2.0f * kPi);

    // 底面の半径を innerRadius〜maxRadius でランダム
    float r = Rnd(minRadius, maxRadius);

    // コーン軸方向でローカル座標を生成
    float h = Rnd(0.0f, height);
    float lx = sinTheta * std::cos(phi) * r;
    float ly = cosTheta * h;           // 軸方向成分
    float lz = sinTheta * std::sin(phi) * r;

    // direction を Z 軸とした回転行列で変換
    Vector3 d = direction;
    float dl = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
    if (dl < 1e-5f) return { lx, ly, lz };
    d = { d.x/dl, d.y/dl, d.z/dl };

    // 直交系構築
    Vector3 up = (std::abs(d.y) < 0.99f) ? Vector3{0,1,0} : Vector3{1,0,0};
    Vector3 right = {
        d.y*up.z - d.z*up.y,
        d.z*up.x - d.x*up.z,
        d.x*up.y - d.y*up.x
    };
    float rl = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
    right = { right.x/rl, right.y/rl, right.z/rl };
    Vector3 fwd = {
        right.y*d.z - right.z*d.y,
        right.z*d.x - right.x*d.z,
        right.x*d.y - right.y*d.x
    };

    return {
        right.x*lx + fwd.x*lz + d.x*ly,
        right.y*lx + fwd.y*lz + d.y*ly,
        right.z*lx + fwd.z*lz + d.z*ly
    };
}

void YEmitterCone::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("コーン方向",       &direction.x,  0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("内側角度 (°) min",  &innerAngle,   0.5f, 0.0f, 89.9f);
    ImGui::DragFloat("外側角度 (°) max",  &outerAngle,   0.5f, 0.0f, 90.0f);
    ImGui::DragFloat("内側半径 min",      &minRadius,    0.01f, 0.0f, 50.0f);
    ImGui::DragFloat("外側半径 max",      &maxRadius,    0.01f, 0.0f, 50.0f);
    ImGui::DragFloat("高さ",             &height,       0.1f,  0.0f, 50.0f);
    if (innerAngle > outerAngle) innerAngle = outerAngle;
    if (minRadius  > maxRadius)  minRadius  = maxRadius;
    ImGui::TextDisabled("用途: 炎噴射 / スラッシュ / ビーム");
#endif
}
