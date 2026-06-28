#include "UpdateSinMovement.h"
#include <cmath>
static constexpr float k2Pi = 6.28318f;

void UpdateSinMovement::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    timeAccum_ += dt;
    float sinVal = std::sin(timeAccum_ * frequency_ * k2Pi + attrs[index].phase);

    // 位置に振幅を加算（速度ではなく直接位置を補正）
    attrs[index].position.x += amplitude_.x * sinVal * dt;
    attrs[index].position.y += amplitude_.y * sinVal * dt;
    attrs[index].position.z += amplitude_.z * sinVal * dt;
}

void UpdateSinMovement::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("振幅 (m)",  &amplitude_.x, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("周波数 (Hz)", &frequency_,  0.1f,  0.1f, 20.0f);
    ImGui::TextDisabled("SpawnPhase と組み合わせて粒子ごとにずらすと自然に見えます");
#endif
}

void UpdateSinMovement::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"amplitude",{amplitude_.x,amplitude_.y,amplitude_.z}},
        {"frequency",frequency_}
    };
}

void UpdateSinMovement::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("amplitude")) { auto v=json["amplitude"]; amplitude_={v[0],v[1],v[2]}; }
    if (json.contains("frequency")) frequency_ = json["frequency"];
}
