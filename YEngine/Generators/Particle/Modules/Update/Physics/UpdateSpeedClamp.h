#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 速度の最大値・最小値をクランプ
/// 用途: 爆発破片が飛びすぎるのを防ぐ、最低速度の保証
/// </summary>
class UpdateSpeedClamp : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "速度クランプ"; }
    std::string GetTypeName() const override { return "UpdateSpeedClamp"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float maxSpeed_ = 20.0f;
    float minSpeed_ = 0.0f;
};
REGISTER_UPDATE_MODULE(UpdateSpeedClamp)
