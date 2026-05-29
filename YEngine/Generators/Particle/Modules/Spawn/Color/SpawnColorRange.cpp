#include "SpawnColorRange.h"

void SpawnColorRange::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    float t = ParticleMath::Random01();
    attrs[index].color = {
        ParticleMath::Lerp(colorA_.x, colorB_.x, t),
        ParticleMath::Lerp(colorA_.y, colorB_.y, t),
        ParticleMath::Lerp(colorA_.z, colorB_.z, t),
        ParticleMath::Lerp(colorA_.w, colorB_.w, t)
    };
}

void SpawnColorRange::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::ColorEdit4("カラー A", &colorA_.x);
    ImGui::ColorEdit4("カラー B", &colorB_.x);
    ImGui::TextDisabled("A と B の間でランダムに補間します");
#endif
}

void SpawnColorRange::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"colorA", {colorA_.x,colorA_.y,colorA_.z,colorA_.w}},
        {"colorB", {colorB_.x,colorB_.y,colorB_.z,colorB_.w}}
    };
}

void SpawnColorRange::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("colorA")) { auto c=json["colorA"]; colorA_={c[0],c[1],c[2],c[3]}; }
    if (json.contains("colorB")) { auto c=json["colorB"]; colorB_={c[0],c[1],c[2],c[3]}; }
}
