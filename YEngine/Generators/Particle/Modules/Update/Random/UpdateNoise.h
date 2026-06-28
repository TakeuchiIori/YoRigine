#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// ランダムなノイズで揺らぎを追加
/// 用途: 炎の揺らぎ、風の影響、自然な動きなど
/// </summary>
class UpdateNoise : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "ノイズ"; }
    std::string GetTypeName() const override { return "UpdateNoise"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    float noiseStrength_ = 1.0f;
    float frequency_ = 1.0f; // ノイズの変化速度
};

REGISTER_UPDATE_MODULE(UpdateNoise)
