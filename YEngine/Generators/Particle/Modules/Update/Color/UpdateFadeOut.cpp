#include "UpdateFadeOut.h"
#include "../../ParticleAttribute.h"

void UpdateFadeOut::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
	(void)dt; // dtはこのモジュールでは使用しない

    float age = attrs[index].GetNormalizedAge();

    if (age >= fadeStart_) {
        // fadeStart_ ~ fadeEnd_ の区間で 1.0 → 0.0 にフェード
        float fadeRange = fadeEnd_ - fadeStart_;
        if (fadeRange > 0.0f) {
            float t = (age - fadeStart_) / fadeRange;
            t = std::clamp(t, 0.0f, 1.0f);
            attrs[index].color.w = 1.0f - t;  // アルファ値を減少
        }
        else {
            attrs[index].color.w = 0.0f;
        }
    }
}

void UpdateFadeOut::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::SliderFloat("フェード開始", &fadeStart_, 0.0f, 1.0f);
    ImGui::SliderFloat("フェード終了", &fadeEnd_, 0.0f, 1.0f);
    if (fadeStart_ > fadeEnd_) {
        fadeStart_ = fadeEnd_;
    }
    ImGui::TextWrapped("開始から終了までの寿命の間にパーティクルがフェードアウトします。");
#endif
}

void UpdateFadeOut::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"fadeStart", fadeStart_},
        {"fadeEnd", fadeEnd_}
    };
}

void UpdateFadeOut::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("fadeStart")) fadeStart_ = json["fadeStart"];
    if (json.contains("fadeEnd")) fadeEnd_ = json["fadeEnd"];
}