#include "SpawnRing.h"
#include <cmath>
static constexpr float kPi = 3.14159265f;

void SpawnRing::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    float angle = ParticleMath::RandomRange(0.0f, 2.0f * kPi);
    float speed = ParticleMath::RandomRange(minSpeed_, maxSpeed_);

    float cx = std::cos(angle), cz = std::sin(angle);

    // 生成位置をリング上に設定
    attrs[index].position.x += cx * radius_;
    attrs[index].position.z += cz * radius_;
    attrs[index].origin      = attrs[index].position;

    // 速度：外向き + 上方バイアス
    attrs[index].velocity = {
        cx * speed,
        upwardBias_ * speed,
        cz * speed
    };
}

void SpawnRing::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("起点半径",      &radius_,     0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("最小速度",      &minSpeed_,   0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("最大速度",      &maxSpeed_,   0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("上方バイアス",  &upwardBias_, 0.01f, 0.0f, 2.0f);
#endif
}

void SpawnRing::SaveToJson(nlohmann::json& json) const
{
    json = { {"minSpeed",minSpeed_},{"maxSpeed",maxSpeed_},
             {"upwardBias",upwardBias_},{"radius",radius_} };
}

void SpawnRing::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minSpeed"))   minSpeed_   = json["minSpeed"];
    if (json.contains("maxSpeed"))   maxSpeed_   = json["maxSpeed"];
    if (json.contains("upwardBias")) upwardBias_ = json["upwardBias"];
    if (json.contains("radius"))     radius_     = json["radius"];
}
