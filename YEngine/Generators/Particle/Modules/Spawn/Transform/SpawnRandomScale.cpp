#include "SpawnRandomScale.h"
#include "../../ParticleAttribute.h"

void SpawnRandomScale::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
        Vector3 scale = ParticleMath::RandomVector3(minScale_, maxScale_);
        attrs[index].scale = scale;
}

void SpawnRandomScale::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::Text("Random Scale Module");
    ImGui::DragFloat3("Min Scale", &minScale_.x, 0.1f, 0.01f, 100.0f);
    ImGui::DragFloat3("Max Scale", &maxScale_.x, 0.1f, 0.01f, 100.0f);
#endif
}

void SpawnRandomScale::SaveToJson(nlohmann::json& json) const
{
    json["minScale"] = {
    {"x", minScale_.x},
    {"y", minScale_.y},
    {"z", minScale_.z}
    };
    json["maxScale"] = {
        {"x",maxScale_.x},
        {"y",maxScale_.y},
        {"z",maxScale_.z}
    };
}

void SpawnRandomScale::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minScale")) {
        minScale_.x = json["minScale"]["x"];
        minScale_.y = json["minScale"]["y"];
        minScale_.z = json["minScale"]["z"];
    }
    if (json.contains("maxScale")) {
        maxScale_.x = json["maxScale"]["x"];
        maxScale_.y = json["maxScale"]["y"];
        maxScale_.z = json["maxScale"]["z"];
    }
}
