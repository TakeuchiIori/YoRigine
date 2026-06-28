#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

class SpawnRandomRotate : public ISpawnModule {
public:
    void OnSpawn(ParticleAttribute* attrs, uint32_t index) override;
    void DrawEditor() override;
    std::string GetName() const override { return "Spawn Rotate"; } // 名前を少し汎用的に変更
    std::string GetTypeName() const override { return "SpawnRandomRotate"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    // 各軸をランダムにするかどうかのフラグ
    bool randomX_ = false;
    bool randomY_ = false;
    bool randomZ_ = false;

    // 固定、またはランダム時のベースとなる角度 (度数法で保持してEditorで使いやすくする)
    Vector3 baseRotation_ = { 0.0f, 0.0f, 0.0f };

    const float kDegToRad = 3.14159265f / 180.0f;
};

REGISTER_SPAWN_MODULE(SpawnRandomRotate)