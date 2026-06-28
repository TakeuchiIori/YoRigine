#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 特定の点にパーティクルを引き寄せる
/// 用途: 吸収エフェクト、収束、ブラックホールなど
/// </summary>
class UpdateAttractor : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "アトラクタ"; }
    std::string GetTypeName() const override { return "UpdateAttractor"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 attractorPosition_ = { 0.0f, 0.0f, 0.0f };
    float strength_ = 5.0f;
    float minDistance_ = 0.5f; // これ以下の距離では力を制限
};

REGISTER_UPDATE_MODULE(UpdateAttractor)
