#include "UpdateScale.h"

void UpdateScale::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    (void)dt;

    float t = attrs[index].GetNormalizedAge();

    // Easingクラスを使用してイージング関数を適用
    float easedT = Easing::Ease(easingFunction_, t);

    attrs[index].scale = Lerp(startScale_, endScale_, easedT);
}

void UpdateScale::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("開始 スケール", &startScale_.x, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat3("終了 スケール", &endScale_.x, 0.01f, 0.0f, 10.0f);

    // すべてのイージング関数を選択可能に
    const char* easingTypes[] = {
        "Linear",
        "EaseInSine", "EaseOutSine", "EaseInOutSine",
        "EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
        "EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
        "EaseInQuart", "EaseOutQuart", "EaseInOutQuart",
        "EaseInQuint", "EaseOutQuint", "EaseInOutQuint",
        "EaseInExpo", "EaseOutExpo", "EaseInOutExpo",
        "EaseInCirc", "EaseOutCirc", "EaseInOutCirc",
        "EaseInBack", "EaseOutBack", "EaseInOutBack",
        "EaseInElastic", "EaseOutElastic", "EaseInOutElastic",
        "EaseInBounce", "EaseOutBounce", "EaseInOutBounce",
        "EaseOutGrowBounce"
    };

    int currentEasing = static_cast<int>(easingFunction_);
    if (ImGui::Combo("Easing Function", &currentEasing, easingTypes, IM_ARRAYSIZE(easingTypes))) {
        easingFunction_ = static_cast<Easing::Function>(currentEasing);
    }
#endif
}

void UpdateScale::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"startScale", {startScale_.x, startScale_.y, startScale_.z}},
        {"endScale", {endScale_.x, endScale_.y, endScale_.z}},
        {"easingFunction", static_cast<int>(easingFunction_)}
    };
}

void UpdateScale::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("startScale")) {
        auto s = json["startScale"];
        startScale_ = { s[0], s[1], s[2] };
    }
    if (json.contains("endScale")) {
        auto s = json["endScale"];
        endScale_ = { s[0], s[1], s[2] };
    }
    if (json.contains("easingFunction")) {
        easingFunction_ = static_cast<Easing::Function>(json["easingFunction"].get<int>());
    }
    // 旧形式(curveType)からの互換性対応
    else if (json.contains("curveType")) {
        int curveType = json["curveType"];
        switch (curveType) {
        case 1: easingFunction_ = Easing::Function::EaseInQuad; break;
        case 2: easingFunction_ = Easing::Function::EaseOutQuad; break;
        case 3: easingFunction_ = Easing::Function::EaseInOutQuad; break;
        default: easingFunction_ = Easing::Function::Linear; break;
        }
    }
}