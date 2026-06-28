#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// ParticleAttribute::angularVelocity を毎フレーム rotation に積算（スピン）
/// SpawnAngularVelocity と組み合わせて使う
/// 用途: 回転する破片、硬貨、葉
/// </summary>
class UpdateAngularVelocity : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "角速度スピン"; }
    std::string GetTypeName() const override { return "UpdateAngularVelocity"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
};
REGISTER_UPDATE_MODULE(UpdateAngularVelocity)
