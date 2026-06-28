#include "SpawnTangentVelocity.h"
#include <cmath>

void SpawnTangentVelocity::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    // origin からの方向ベクトルを速度方向に使用
    Vector3 d = {
        attrs[index].position.x - attrs[index].origin.x,
        attrs[index].position.y - attrs[index].origin.y,
        attrs[index].position.z - attrs[index].origin.z
    };
    float len = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
    if (len < 1e-5f) {
        // 原点と同位置なら球面上のランダム方向
        float phi   = ParticleMath::RandomRange(0.0f, 6.2832f);
        float theta = ParticleMath::RandomRange(0.0f, 3.1416f);
        d = { std::sin(theta)*std::cos(phi), std::sin(theta)*std::sin(phi), std::cos(theta) };
        len = 1.0f;
    }
    float speed = ParticleMath::RandomRange(minSpeed_, maxSpeed_);
    attrs[index].velocity = { d.x/len*speed, d.y/len*speed, d.z/len*speed };
}

void SpawnTangentVelocity::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("最小速度", &minSpeed_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("最大速度", &maxSpeed_, 0.1f, 0.0f, 100.0f);
    ImGui::TextDisabled("球形エミッタと組み合わせて使用すると効果的");
#endif
}

void SpawnTangentVelocity::SaveToJson(nlohmann::json& json) const
{
    json = { {"minSpeed",minSpeed_},{"maxSpeed",maxSpeed_} };
}

void SpawnTangentVelocity::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("minSpeed")) minSpeed_ = json["minSpeed"];
    if (json.contains("maxSpeed")) maxSpeed_ = json["maxSpeed"];
}
