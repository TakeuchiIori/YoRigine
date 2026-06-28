#include "SpawnBurst.h"

void SpawnBurst::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    attrs[index].lifeTime = ParticleMath::RandomRange(minLife_, maxLife_);
}

void SpawnBurst::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("最小寿命 (秒)", &minLife_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("最大寿命 (秒)", &maxLife_, 0.01f, 0.01f, 10.0f);
#endif
}

void SpawnBurst::SaveToJson(nlohmann::json& json) const
{
    json = { {"minLife",minLife_},{"maxLife",maxLife_} };
}

void SpawnBurst::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minLife")) minLife_ = json["minLife"];
    if (json.contains("maxLife")) maxLife_ = json["maxLife"];
}
