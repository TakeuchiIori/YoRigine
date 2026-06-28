#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// 生成時にランダムなスケールを設定
/// 用途: サイズにバリエーションを持たせる
/// </summary>
class SpawnRandomScale : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName() const override { return "Spawn Random Scale"; }
    std::string GetTypeName() const override { return "SpawnRandomScale"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector3 minScale_ = {1.0f,1.0f,1.0f};
    Vector3 maxScale_ = {1.0f,1.0f,1.0f};
};

REGISTER_SPAWN_MODULE(SpawnRandomScale)
