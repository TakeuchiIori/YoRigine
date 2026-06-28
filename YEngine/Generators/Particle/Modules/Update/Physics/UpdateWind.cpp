#include "UpdateWind.h"
#include <cmath>

void UpdateWind::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    timeAccum_ += dt;
    // ガスト：sin 関数で周期的な強さ変化
    float gustMult = 1.0f + gustStrength_ * std::sin(timeAccum_ * 2.3f + attrs[index].phase);

    auto& a = attrs[index];
    a.velocity.x += windForce_.x * gustMult * dt;
    a.velocity.y += windForce_.y * gustMult * dt;
    a.velocity.z += windForce_.z * gustMult * dt;
}

void UpdateWind::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("風力 (m/s²)", &windForce_.x,   0.1f);
    ImGui::DragFloat("ガスト強度",   &gustStrength_,  0.01f, 0.0f, 2.0f);
    ImGui::TextDisabled("SpawnPhase と組み合わせると粒子ごとにタイミングがずれます");
#endif
}

void UpdateWind::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"windForce",{windForce_.x,windForce_.y,windForce_.z}},
        {"gustStrength",gustStrength_}
    };
}

void UpdateWind::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("windForce"))    { auto v=json["windForce"]; windForce_={v[0],v[1],v[2]}; }
    if (json.contains("gustStrength")) gustStrength_ = json["gustStrength"];
}
