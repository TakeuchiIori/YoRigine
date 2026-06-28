#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 地面や平面との衝突処理
/// 用途: バウンド、反射エフェクトなど
/// </summary>
class UpdateCollision : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "衝突"; }
    std::string GetTypeName() const override { return "UpdateCollision"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    float groundHeight_ = 0.0f;
    float bounciness_ = 0.5f;  // 反発係数 (0-1)
    bool killOnCollision_ = false;
};

REGISTER_UPDATE_MODULE(UpdateCollision)
