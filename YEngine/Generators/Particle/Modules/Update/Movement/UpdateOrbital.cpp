#include "UpdateOrbital.h"

void UpdateOrbital::Initialize(uint32_t maxParticles)
{
    initialAngle_.resize(maxParticles);
    for (uint32_t i = 0; i < maxParticles; ++i) {
        initialAngle_[i] = ParticleMath::RandomRange(0.0f, 2.0f * 3.14159265f);
    }
}

void UpdateOrbital::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    (void)dt;
    // 経過時間に基づいて角度を計算
    float currentAngle = initialAngle_[index] + orbitalSpeed_ * attrs[index].GetNormalizedAge();
    
    // 円軌道上の位置を計算
    Vector3 orbitalPosition = center_;
    orbitalPosition.x += cosf(currentAngle) * radius_;
    orbitalPosition.z += sinf(currentAngle) * radius_;
    
    // 位置を直接設定（既存の速度ベースの移動を上書き）
    attrs[index].position = orbitalPosition;
}

void UpdateOrbital::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("中心", &center_.x, 0.1f);
    ImGui::DragFloat("軌道速度", &orbitalSpeed_, 0.1f, -10.0f, 10.0f);
    ImGui::DragFloat("半径", &radius_, 0.1f, 0.1f, 50.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "円形の軌道運動を生成");
#endif
}

void UpdateOrbital::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"center", {center_.x, center_.y, center_.z}},
        {"orbitalSpeed", orbitalSpeed_},
        {"radius", radius_}
    };
}

void UpdateOrbital::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("center")) {
        auto c = json["center"];
        center_ = { c[0], c[1], c[2] };
    }
    if (json.contains("orbitalSpeed")) orbitalSpeed_ = json["orbitalSpeed"];
    if (json.contains("radius")) radius_ = json["radius"];
}
