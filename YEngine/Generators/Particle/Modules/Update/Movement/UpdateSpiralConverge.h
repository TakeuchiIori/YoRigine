#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// スパイラル状に収束する動き
/// 用途: チャージエフェクト、収束ビーム、魔法の溜めなど
/// </summary>
class UpdateSpiralConverge : public IUpdateModule {
public:
    void Initialize(uint32_t maxParticles) override;
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "スパイラル収束"; }
    std::string GetTypeName() const override { return "UpdateSpiralConverge"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 targetPoint_ = { 0.0f, 0.0f, 0.0f };
    float spiralSpeed_ = 3.0f;
    float convergeSpeed_ = 2.0f;
    float spiralRadius_ = 2.0f;
    std::vector<float> particleAngle_;
};

REGISTER_UPDATE_MODULE(UpdateSpiralConverge)
