#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

#include "Vector2.h"

class UpdateUVScroll : public IUpdateModule {
public:

    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "UVスクロール"; }
    std::string GetTypeName() const override { return "UpdateUVScroll"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector2 scrollSpeed = { 0.5f, 0.0f }; // 1秒間にどれだけずらすか
};