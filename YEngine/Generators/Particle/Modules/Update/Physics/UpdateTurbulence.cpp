#include "UpdateTurbulence.h"
#include <cmath>

// ── シンプルなハッシュノイズ ────────────────────────────────────────────────

float UpdateTurbulence::Hash(float n)
{
    // frac(sin(n) * 43758.5453)
    float s = std::sin(n) * 43758.5453f;
    return s - std::floor(s);
}

float UpdateTurbulence::Noise3(float x, float y, float z)
{
    // セル格子補間
    float ix = std::floor(x), iy = std::floor(y), iz = std::floor(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    // smoothstep
    float ux = fx*fx*(3-2*fx), uy = fy*fy*(3-2*fy), uz = fz*fz*(3-2*fz);

    float n = ix + iy*157.0f + iz*113.0f;
    float a = Hash(n);
    float b = Hash(n+1);
    float c = Hash(n+157);
    float d = Hash(n+158);
    float e = Hash(n+113);
    float f = Hash(n+114);
    float g = Hash(n+270);
    float h = Hash(n+271);

    return a + (b-a)*ux + (c-a)*uy + (e-a)*uz
             + (a-b-c+d)*ux*uy + (a-c-e+g)*uy*uz + (a-b-e+f)*ux*uz
             + (-a+b+c-d+e-f-g+h)*ux*uy*uz;
}

void UpdateTurbulence::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    timeAccum_ += dt * speed_;

    auto& a = attrs[index];
    float px = a.position.x * frequency_ + timeAccum_;
    float py = a.position.y * frequency_ + timeAccum_ * 1.31f;
    float pz = a.position.z * frequency_ + timeAccum_ * 0.71f;

    // 各軸のノイズ（オフセットをずらして独立させる）
    float nx = (Noise3(px,       py,       pz      ) - 0.5f) * 2.0f;
    float ny = (Noise3(px+31.7f, py+17.3f, pz+5.1f) - 0.5f) * 2.0f;
    float nz = (Noise3(px+73.1f, py+41.9f, pz+83.7f)- 0.5f) * 2.0f;

    a.velocity.x += nx * strength_ * dt;
    a.velocity.y += ny * strength_ * dt;
    a.velocity.z += nz * strength_ * dt;
}

void UpdateTurbulence::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("強さ",     &strength_,  0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("周波数",   &frequency_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("変化速度", &speed_,     0.01f, 0.0f, 5.0f);
#endif
}

void UpdateTurbulence::SaveToJson(nlohmann::json& json) const
{
    json = { {"strength",strength_},{"frequency",frequency_},{"speed",speed_} };
}

void UpdateTurbulence::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("strength"))  strength_  = json["strength"];
    if (json.contains("frequency")) frequency_ = json["frequency"];
    if (json.contains("speed"))     speed_     = json["speed"];
}
