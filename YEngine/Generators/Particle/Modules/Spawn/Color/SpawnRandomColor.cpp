#include "SpawnRandomColor.h"
#include "../../ParticleAttribute.h"

void SpawnRandomColor::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    attrs[index].color = {
        ParticleMath::RandomRange(minColor_.x, maxColor_.x),
        ParticleMath::RandomRange(minColor_.y, maxColor_.y),
        ParticleMath::RandomRange(minColor_.z, maxColor_.z),
        ParticleMath::RandomRange(minColor_.w, maxColor_.w)
    };
}

void SpawnRandomColor::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::ColorEdit4("最低 色", &minColor_.x);
    ImGui::ColorEdit4("最大 色", &maxColor_.x);
#endif
}

void SpawnRandomColor::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"minColor", {minColor_.x, minColor_.y, minColor_.z, minColor_.w}},
        {"maxColor", {maxColor_.x, maxColor_.y, maxColor_.z, maxColor_.w}}
    };
}

void SpawnRandomColor::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minColor")) {
        auto c = json["minColor"];
        minColor_ = { c[0], c[1], c[2], c[3] };
    }
    if (json.contains("maxColor")) {
        auto c = json["maxColor"];
        maxColor_ = { c[0], c[1], c[2], c[3] };
    }
}
