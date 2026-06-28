#include "UpdateAttractor.h"
#include "MathFunc.h"

void UpdateAttractor::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    Vector3 toAttractor = attractorPosition_ - attrs[index].position;
    float distance = Length(toAttractor);
    
    if (distance > minDistance_) {
        Vector3 direction = Normalize(toAttractor);
        
        // 距離の2乗に反比例する力（重力的）
        float force = strength_ / (distance * distance);
        
        // 速度に加算
        attrs[index].velocity = attrs[index].velocity + direction * force * dt;
    }
}

void UpdateAttractor::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("引力点の位置", &attractorPosition_.x, 0.1f);
    ImGui::DragFloat("強度", &strength_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("最小距離", &minDistance_, 0.1f, 0.0f, 10.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "力 = 強度 / 距離^2");
#endif
}

void UpdateAttractor::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"attractorPosition", {attractorPosition_.x, attractorPosition_.y, attractorPosition_.z}},
        {"strength", strength_},
        {"minDistance", minDistance_}
    };
}

void UpdateAttractor::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("attractorPosition")) {
        auto p = json["attractorPosition"];
        attractorPosition_ = { p[0], p[1], p[2] };
    }
    if (json.contains("strength")) strength_ = json["strength"];
    if (json.contains("minDistance")) minDistance_ = json["minDistance"];
}
