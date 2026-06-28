#include "SpawnSize.h"

void SpawnSize::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    float s = ParticleMath::RandomRange(minSize_, maxSize_);
    attrs[index].scale = { s, s, s };
}

void SpawnSize::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("最小サイズ", &minSize_, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("最大サイズ", &maxSize_, 0.01f, 0.0f, 100.0f);
#endif
}

void SpawnSize::SaveToJson(nlohmann::json& json) const
{
    json = { {"minSize",minSize_},{"maxSize",maxSize_} };
}

void SpawnSize::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minSize")) minSize_ = json["minSize"];
    if (json.contains("maxSize")) maxSize_ = json["maxSize"];
}
