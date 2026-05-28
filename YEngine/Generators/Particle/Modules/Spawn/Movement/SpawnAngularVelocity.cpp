#include "SpawnAngularVelocity.h"

void SpawnAngularVelocity::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    attrs[index].angularVelocity = ParticleMath::RandomVector3(minAngVel_, maxAngVel_);
}

void SpawnAngularVelocity::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("最小角速度 (°/s)", &minAngVel_.x, 1.0f);
    ImGui::DragFloat3("最大角速度 (°/s)", &maxAngVel_.x, 1.0f);
#endif
}

void SpawnAngularVelocity::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"minAngVel", {minAngVel_.x, minAngVel_.y, minAngVel_.z}},
        {"maxAngVel", {maxAngVel_.x, maxAngVel_.y, maxAngVel_.z}}
    };
}

void SpawnAngularVelocity::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minAngVel")) { auto v=json["minAngVel"]; minAngVel_={v[0],v[1],v[2]}; }
    if (json.contains("maxAngVel")) { auto v=json["maxAngVel"]; maxAngVel_={v[0],v[1],v[2]}; }
}
