#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

class UpdateColor : public IUpdateModule
{
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName() const override { return "色"; }
    std::string  GetTypeName() const override { return "UpdateColor"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;
private:
    Vector4 startColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 endColor_{ 1.0f, 0.0f, 0.0f, 0.0f };
};
// ファクトリーに登録
REGISTER_UPDATE_MODULE(UpdateColor)