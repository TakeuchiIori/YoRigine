#include "UpdateAngularVelocity.h"

void UpdateAngularVelocity::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    auto& a = attrs[index];
    a.rotation.x += a.angularVelocity.x * dt;
    a.rotation.y += a.angularVelocity.y * dt;
    a.rotation.z += a.angularVelocity.z * dt;
}

void UpdateAngularVelocity::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::TextDisabled("SpawnAngularVelocity で設定した角速度を rotation に積算");
    ImGui::TextDisabled("スピン速度は SpawnAngularVelocity 側で調整してください");
#endif
}

void UpdateAngularVelocity::SaveToJson(nlohmann::json& json) const { json = {}; }
void UpdateAngularVelocity::LoadFromJson(const nlohmann::json&) {}
