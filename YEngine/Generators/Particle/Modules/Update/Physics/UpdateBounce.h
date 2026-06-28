#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 水平面（Y = floorY）でバウンス反射
/// 用途: 破片の床バウンス、水滴、血しぶき
/// </summary>
class UpdateBounce : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "床バウンス"; }
    std::string GetTypeName() const override { return "UpdateBounce"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float floorY_         = 0.0f;   // 床の Y 座標
    float restitution_    = 0.4f;   // 跳ね返り係数 [0=吸収, 1=完全弾性]
    float friction_       = 0.3f;   // 水平摩擦係数
    float minBounceSpeed_ = 0.5f;   // これ以下の Y 速度ではバウンスしない
};
REGISTER_UPDATE_MODULE(UpdateBounce)
