#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 生成位置 (origin) から指定距離を超えたパーティクルを即消滅させる
/// 用途: 爆発の破片が一定範囲以上飛ばないようにする、局所エフェクト
/// </summary>
class UpdateLimitByDistance : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "距離制限"; }
    std::string GetTypeName() const override { return "UpdateLimitByDistance"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float maxDistance_ = 5.0f;
};
REGISTER_UPDATE_MODULE(UpdateLimitByDistance)
