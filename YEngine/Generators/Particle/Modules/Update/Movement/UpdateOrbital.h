#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 円軌道運動を追加
/// 用途: 周回エフェクト、オーブ、衛星など
/// </summary>
class UpdateOrbital : public IUpdateModule {
public:
    void Initialize(uint32_t maxParticles) override;
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "軌道"; }
    std::string GetTypeName() const override { return "UpdateOrbital"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 center_ = { 0.0f, 0.0f, 0.0f };
    float orbitalSpeed_ = 2.0f;
    float radius_ = 3.0f;
    std::vector<float> initialAngle_; // 各パーティクルの初期角度
};

REGISTER_UPDATE_MODULE(UpdateOrbital)
