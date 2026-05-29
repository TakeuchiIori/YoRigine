#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 一定方向の風力を加速度として加算
/// 用途: 雨・雪・砂塵・煙の流れ
/// </summary>
class UpdateWind : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "風"; }
    std::string GetTypeName() const override { return "UpdateWind"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 windForce_ = { 2.0f, 0.0f, 0.0f }; // m/s²
    float   gustStrength_ = 0.5f;  // ランダムなガスト（突風）強度
    float   timeAccum_    = 0.0f;
};
REGISTER_UPDATE_MODULE(UpdateWind)
