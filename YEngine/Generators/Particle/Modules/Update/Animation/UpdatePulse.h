#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// パルス状にスケールを変化
/// 用途: ヒットエフェクト、インパクト、鼓動など
/// </summary>
class UpdatePulse : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "パルス"; }
    std::string GetTypeName() const override { return "UpdatePulse"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    float pulseFrequency_ = 5.0f;  // パルスの周波数
    float pulseAmplitude_ = 0.5f;  // パルスの強さ
    float baseScale_ = 1.0f;       // 基準スケール
};

REGISTER_UPDATE_MODULE(UpdatePulse)
