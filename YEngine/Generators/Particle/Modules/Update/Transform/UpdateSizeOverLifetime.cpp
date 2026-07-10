#include "UpdateSizeOverLifetime.h"

UpdateSizeOverLifetime::UpdateSizeOverLifetime()
{
    // デフォルト: 1.0 から 0.0 へ smoothstep で縮む（広がって消える系の基本形）
    curve_.keys_   = { {0.0f, 1.0f}, {1.0f, 0.0f} };
    curve_.interp_ = ParticleCurve::Interp::Smooth;
}

void UpdateSizeOverLifetime::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    float t = attrs[index].GetNormalizedAge();
    float mult = curve_.Evaluate(t);

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
    ImGui::TextDisabled("生成時スケール × カーブ値 で実スケールを決定");
    curve_.DrawEditor("サイズ倍率カーブ", 0.0f, 3.0f, ImVec2(0, 120));
#endif
}

void UpdateSizeOverLifetime::SaveToJson(nlohmann::json& json) const
{
    json = nlohmann::json::object();
    curve_.SaveToJson(json["curve"]);
}

void UpdateSizeOverLifetime::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("curve")) {
        curve_.LoadFromJson(json["curve"]);
        return;
    }

    // 後方互換: 旧 startScale / endScale / useCurve 形式を 2 点カーブへ変換
    float startScale = json.value("startScale", 1.0f);
    float endScale   = json.value("endScale",   0.0f);
    bool  smooth     = json.value("useCurve",   true);
    curve_.keys_   = { {0.0f, startScale}, {1.0f, endScale} };
    curve_.interp_ = smooth ? ParticleCurve::Interp::Smooth : ParticleCurve::Interp::Linear;
}
