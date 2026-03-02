#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// パーティクルを回転させる
/// 用途: 回転するエフェクト、竜巻、渦など
/// </summary>
class UpdateRotation : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "回転"; }
    std::string GetTypeName() const override { return "UpdateRotation"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 rotationSpeed_ = { 0.0f, 180.0f, 0.0f }; // 度/秒
};

REGISTER_UPDATE_MODULE(UpdateRotation)
