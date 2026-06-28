#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 速さに応じてカラーを補間（速い = 熱い色、遅い = 冷えた色 等）
/// 用途: 高速な火花は白く輝き、失速すると赤くなる
/// </summary>
class UpdateColorBySpeed : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "速度でカラー変化"; }
    std::string GetTypeName() const override { return "UpdateColorBySpeed"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector4 slowColor_ = { 0.8f, 0.1f, 0.0f, 1.0f }; // 低速時
    Vector4 fastColor_ = { 1.0f, 1.0f, 0.8f, 1.0f }; // 高速時
    float   slowSpeed_ = 0.0f;
    float   fastSpeed_ = 15.0f;
};
REGISTER_UPDATE_MODULE(UpdateColorBySpeed)
