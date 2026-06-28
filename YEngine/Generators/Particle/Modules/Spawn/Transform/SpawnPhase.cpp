#include "SpawnPhase.h"
static constexpr float k2Pi = 6.28318f;

void SpawnPhase::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    attrs[index].phase = ParticleMath::RandomRange(0.0f, k2Pi);
}

void SpawnPhase::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::TextDisabled("生成時に [0, 2pi] のランダム位相を設定します");
    ImGui::TextDisabled("UpdateSinMovement と組み合わせて使用してください");
#endif
}

void SpawnPhase::SaveToJson(nlohmann::json& json) const { json = {}; }
void SpawnPhase::LoadFromJson(const nlohmann::json&) {}
