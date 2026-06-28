#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// サイン波で位置を振動させる（ふわふわ浮遊感）
/// SpawnPhase と組み合わせると粒子ごとにタイミングがずれる
/// 用途: 魔法の光の粒、精霊、浮遊エフェクト
/// </summary>
class UpdateSinMovement : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "サイン波振動"; }
    std::string GetTypeName() const override { return "UpdateSinMovement"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 amplitude_  = { 0.0f, 0.3f, 0.0f }; // 振幅 (m)
    float   frequency_  = 2.0f;   // 周波数 (Hz)
    float   timeAccum_  = 0.0f;
};
REGISTER_UPDATE_MODULE(UpdateSinMovement)
