#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 指定軸を中心とした渦流（竜巻・水流・魔法陣回転）
/// 用途: 竜巻、水の渦、魔法詠唱
/// </summary>
class UpdateVortex : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "渦流"; }
    std::string GetTypeName() const override { return "UpdateVortex"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector3 center_       = { 0.0f, 0.0f, 0.0f };
    Vector3 axis_         = { 0.0f, 1.0f, 0.0f }; // 回転軸
    float   angularSpeed_ = 180.0f; // 度/秒
    float   attractForce_ = 0.5f;   // 中心への引力
};
REGISTER_UPDATE_MODULE(UpdateVortex)
