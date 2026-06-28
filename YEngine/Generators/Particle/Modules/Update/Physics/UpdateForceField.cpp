#include "UpdateForceField.h"
#include <cmath>
#include <algorithm>

void UpdateForceField::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    auto& a = attrs[index];
    float dx = fieldCenter_.x - a.position.x;
    float dy = fieldCenter_.y - a.position.y;
    float dz = fieldCenter_.z - a.position.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (dist < 1e-4f || dist > maxRange_) return;

    float force = strength_ / std::pow(dist, falloff_);
    float invD  = 1.0f / dist;
    a.velocity.x += dx * invD * force * dt;
    a.velocity.y += dy * invD * force * dt;
    a.velocity.z += dz * invD * force * dt;
}

void UpdateForceField::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("中心位置",     &fieldCenter_.x, 0.1f);
    ImGui::DragFloat("強さ(+引/-斥)", &strength_,      0.1f, -50.0f, 50.0f);
    ImGui::DragFloat("距離減衰指数",  &falloff_,       0.1f, 0.5f, 5.0f);
    ImGui::DragFloat("最大影響範囲",  &maxRange_,      0.1f, 0.1f, 100.0f);
#endif
}

void UpdateForceField::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"fieldCenter",{fieldCenter_.x,fieldCenter_.y,fieldCenter_.z}},
        {"strength",strength_},{"falloff",falloff_},{"maxRange",maxRange_}
    };
}

void UpdateForceField::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("fieldCenter")) { auto v=json["fieldCenter"]; fieldCenter_={v[0],v[1],v[2]}; }
    if (json.contains("strength"))  strength_ = json["strength"];
    if (json.contains("falloff"))   falloff_  = json["falloff"];
    if (json.contains("maxRange"))  maxRange_ = json["maxRange"];
}
