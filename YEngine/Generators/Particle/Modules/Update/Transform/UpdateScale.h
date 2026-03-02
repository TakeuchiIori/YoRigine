#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include "Easing.h"

/// <summary>
/// 時間経過でスケールを変化
/// 用途: フェードイン/アウト、成長/収縮エフェクト
/// </summary>
class UpdateScale : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "スケール"; }
    std::string GetTypeName() const override { return "UpdateScale"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 startScale_ = { 0.1f, 0.1f, 0.1f };
    Vector3 endScale_ = { 2.0f, 2.0f, 2.0f };
    Easing::Function easingFunction_ = Easing::Function::Linear;
};

REGISTER_UPDATE_MODULE(UpdateScale)