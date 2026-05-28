#include "UpdateSizeOverLifetime.h"
#include <cmath>

void UpdateSizeOverLifetime::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    float t = attrs[index].GetNormalizedAge();
    if (useCurve_) t = t * t * (3.0f - 2.0f * t); // smoothstep

    float mult = ParticleMath::Lerp(startScale_, endScale_, t);

    // initialScale を基準に乗算
    attrs[index].scale = {
        attrs[index].initialScale.x * mult,
        attrs[index].initialScale.y * mult,
        attrs[index].initialScale.z * mult
    };
}

void UpdateSizeOverLifetime::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("開始スケール倍率", &startScale_, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("終了スケール倍率", &endScale_,   0.01f, 0.0f, 5.0f);
    ImGui::Checkbox("スムーズ補間", &useCurve_);
    ImGui::TextDisabled("生成時スケール × 倍率 で実スケールを決定");
#endif
}

void UpdateSizeOverLifetime::SaveToJson(nlohmann::json& json) const
{
    json = { {"startScale",startScale_},{"endScale",endScale_},{"useCurve",useCurve_} };
}

void UpdateSizeOverLifetime::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("startScale")) startScale_ = json["startScale"];
    if (json.contains("endScale"))   endScale_   = json["endScale"];
    if (json.contains("useCurve"))   useCurve_   = json["useCurve"];
}
