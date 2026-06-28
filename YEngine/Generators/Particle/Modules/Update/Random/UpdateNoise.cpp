#include "UpdateNoise.h"

void UpdateNoise::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    // シンプルなランダムノイズ（実際にはパーリンノイズ等を使うとより自然）
    Vector3 noise = {
        ParticleMath::RandomRange(-1.0f, 1.0f),
        ParticleMath::RandomRange(-1.0f, 1.0f),
        ParticleMath::RandomRange(-1.0f, 1.0f)
    };
    
    // 周波数を考慮したノイズ適用
    Vector3 force = noise * noiseStrength_ * frequency_;
    
    attrs[index].velocity = attrs[index].velocity + force * dt;
}

void UpdateNoise::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("Noise Strength", &noiseStrength_, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("Frequency", &frequency_, 0.1f, 0.1f, 10.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "Adds random turbulence to particles");
#endif
}

void UpdateNoise::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"noiseStrength", noiseStrength_},
        {"frequency", frequency_}
    };
}

void UpdateNoise::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("noiseStrength")) noiseStrength_ = json["noiseStrength"];
    if (json.contains("frequency")) frequency_ = json["frequency"];
}
