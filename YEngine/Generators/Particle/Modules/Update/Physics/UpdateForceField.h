#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// ワールド上の一点からの引力 or 斥力フィールド
/// 用途: ブラックホール、磁石、魔法の引き寄せ、爆風
/// </summary>
class UpdateForceField : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "フォースフィールド"; }
    std::string GetTypeName() const override { return "UpdateForceField"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 fieldCenter_ = { 0.0f, 0.0f, 0.0f };
    float   strength_    = 5.0f;   // 正=引力, 負=斥力
    float   falloff_     = 2.0f;   // 距離の何乗で減衰 (2=逆2乗)
    float   maxRange_    = 10.0f;  // 影響範囲
};
REGISTER_UPDATE_MODULE(UpdateForceField)
