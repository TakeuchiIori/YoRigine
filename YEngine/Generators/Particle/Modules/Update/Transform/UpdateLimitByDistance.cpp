#include "UpdateLimitByDistance.h"
#include <cmath>

void UpdateLimitByDistance::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    auto& a = attrs[index];
    float dx = a.position.x - a.origin.x;
    float dy = a.position.y - a.origin.y;
    float dz = a.position.z - a.origin.z;
    if (dx*dx + dy*dy + dz*dz > maxDistance_*maxDistance_) {
        a.isActive = false;
    }
}

void UpdateLimitByDistance::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("最大距離 (m)", &maxDistance_, 0.1f, 0.0f, 100.0f);
#endif
}

void UpdateLimitByDistance::SaveToJson(nlohmann::json& json) const
{
    json = { {"maxDistance",maxDistance_} };
}

void UpdateLimitByDistance::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("maxDistance")) maxDistance_ = json["maxDistance"];
}
