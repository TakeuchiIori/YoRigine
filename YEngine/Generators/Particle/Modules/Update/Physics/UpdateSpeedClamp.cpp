#include "UpdateSpeedClamp.h"
#include <cmath>
#include <algorithm>

void UpdateSpeedClamp::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    auto& a  = attrs[index];
    float spd = a.GetSpeed();
    if (spd < 1e-6f) return;

    float clamped = std::clamp(spd, minSpeed_, maxSpeed_);
    if (std::abs(clamped - spd) > 1e-6f) {
        float scale = clamped / spd;
        a.velocity.x *= scale;
        a.velocity.y *= scale;
        a.velocity.z *= scale;
    }
}

void UpdateSpeedClamp::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("最小速度", &minSpeed_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("最大速度", &maxSpeed_, 0.1f, 0.0f, 100.0f);
#endif
}

void UpdateSpeedClamp::SaveToJson(nlohmann::json& json) const
{
    json = { {"minSpeed",minSpeed_},{"maxSpeed",maxSpeed_} };
}

void UpdateSpeedClamp::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minSpeed")) minSpeed_ = json["minSpeed"];
    if (json.contains("maxSpeed")) maxSpeed_ = json["maxSpeed"];
}
