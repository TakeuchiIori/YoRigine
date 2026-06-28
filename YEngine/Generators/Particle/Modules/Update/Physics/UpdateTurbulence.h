#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// ハッシュベース 3D 乱流場で速度を乱す（有機的な揺れ）
/// 用途: 煙、霧、ちり、魔法のパーティクル
/// </summary>
class UpdateTurbulence : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "乱流"; }
    std::string GetTypeName() const override { return "UpdateTurbulence"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    float strength_  = 2.0f;  // 乱流の強さ
    float frequency_ = 1.0f;  // 乱流のスケール（小さいほど大きな渦）
    float speed_     = 0.5f;  // 時間方向の変化速度
    float timeAccum_ = 0.0f;  // 内部時間

    static float Hash(float n);
    static float Noise3(float x, float y, float z);
};
REGISTER_UPDATE_MODULE(UpdateTurbulence)
