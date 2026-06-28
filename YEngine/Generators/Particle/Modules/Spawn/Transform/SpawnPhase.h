#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// UpdateSinMovement 等の波動系モジュール向けランダム位相オフセット
/// 用途: ふわふわ浮遊、ゆらゆら揺れ（全粒子が同一タイミングにならないよう）
/// </summary>
class SpawnPhase : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "位相オフセット"; }
    std::string GetTypeName() const override { return "SpawnPhase"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
};
REGISTER_SPAWN_MODULE(SpawnPhase)
