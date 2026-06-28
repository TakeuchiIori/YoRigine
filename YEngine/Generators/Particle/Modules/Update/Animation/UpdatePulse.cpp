#include "UpdatePulse.h"

void UpdatePulse::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    (void)dt;
    
    // サイン波でパルスを生成
    float pulse = sinf(attrs[index].GetNormalizedAge() * pulseFrequency_ * 2.0f * 3.14159265f);
    
    // スケールを計算
    float scale = baseScale_ + pulse * pulseAmplitude_;
    
    attrs[index].scale = { scale, scale, scale };
}

void UpdatePulse::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("パルス周波数", &pulseFrequency_, 0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("パルス振幅", &pulseAmplitude_, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("基本スケール", &baseScale_, 0.01f, 0.1f, 5.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "脈動・鼓動エフェクトを生成");
#endif
}

void UpdatePulse::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"pulseFrequency", pulseFrequency_},
        {"pulseAmplitude", pulseAmplitude_},
        {"baseScale", baseScale_}
    };
}

void UpdatePulse::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("pulseFrequency")) pulseFrequency_ = json["pulseFrequency"];
    if (json.contains("pulseAmplitude")) pulseAmplitude_ = json["pulseAmplitude"];
    if (json.contains("baseScale")) baseScale_ = json["baseScale"];
}
