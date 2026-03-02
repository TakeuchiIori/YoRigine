#include "SpawnLifeTime.h"
#include "../../ParticleAttribute.h"

void SpawnLifeTime::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    attrs[index].lifeTime = ParticleMath::RandomRange(minLifeTime_, maxLifeTime_);
    attrs[index].currentTime = 0.0f;
}

void SpawnLifeTime::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("最低 生存時間", &minLifeTime_, 0.1f, 0.1f, 10.0f);
    ImGui::DragFloat("最大 生存時間", &maxLifeTime_, 0.1f, 0.1f, 10.0f);
    if (minLifeTime_ > maxLifeTime_) {
        minLifeTime_ = maxLifeTime_;
    }
#endif
}

void SpawnLifeTime::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"minLifeTime", minLifeTime_},
        {"maxLifeTime", maxLifeTime_}
    };
}

void SpawnLifeTime::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minLifeTime")) minLifeTime_ = json["minLifeTime"];
    if (json.contains("maxLifeTime")) maxLifeTime_ = json["maxLifeTime"];
}