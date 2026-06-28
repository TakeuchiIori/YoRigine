#include "SpawnUV.h"

void SpawnUV::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    attrs[index].uvScale = initialScale;
    attrs[index].uvOffset = initialOffset;
}

void SpawnUV::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat2("スケール", &initialScale.x, 0.1f);
    ImGui::DragFloat2("オフセット", &initialOffset.x, 0.1f);
#endif
}

void SpawnUV::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"initialScale", {{"x", initialScale.x}, {"y", initialScale.y}}},
        {"initialOffset", {{"x", initialOffset.x}, {"y", initialOffset.y}}}
	};
}

void SpawnUV::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("initialScale") && json["initialScale"].is_object()) {
        const auto& scaleJson = json["initialScale"];
        if (scaleJson.contains("x") && scaleJson.contains("y")) {
            initialScale.x = scaleJson["x"].get<float>();
            initialScale.y = scaleJson["y"].get<float>();
        }
    }
    if (json.contains("initialOffset") && json["initialOffset"].is_object()) {
        const auto& offsetJson = json["initialOffset"];
        if (offsetJson.contains("x") && offsetJson.contains("y")) {
            initialOffset.x = offsetJson["x"].get<float>();
            initialOffset.y = offsetJson["y"].get<float>();
        }
	}
}
