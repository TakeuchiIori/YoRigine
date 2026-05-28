#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 速度方向に scale.y（または X/Z）を引き伸ばす（モーションブラー・火花・流星）
/// rotation も速度方向に向けるので billboardType = None の場合に有効
/// 用途: 火花、弾丸トレイル、雨滴
/// </summary>
class UpdateStretchByVelocity : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "速度方向引き伸ばし"; }
    std::string GetTypeName() const override { return "UpdateStretchByVelocity"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float stretchFactor_ = 0.5f;  // 速度に乗じる引き伸ばし係数
    float maxStretch_    = 5.0f;  // 最大引き伸ばし倍率
};
REGISTER_UPDATE_MODULE(UpdateStretchByVelocity)
