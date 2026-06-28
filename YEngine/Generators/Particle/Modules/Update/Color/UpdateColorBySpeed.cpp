#include "UpdateColorBySpeed.h"
#include <algorithm>

void UpdateColorBySpeed::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    float spd   = attrs[index].GetSpeed();
    float range = fastSpeed_ - slowSpeed_;
    float t     = (range > 1e-5f) ? std::clamp((spd - slowSpeed_) / range, 0.0f, 1.0f) : 0.0f;

    attrs[index].color = {
        slowColor_.x + (fastColor_.x - slowColor_.x) * t,
        slowColor_.y + (fastColor_.y - slowColor_.y) * t,
        slowColor_.z + (fastColor_.z - slowColor_.z) * t,
        slowColor_.w + (fastColor_.w - slowColor_.w) * t
    };
}

void UpdateColorBySpeed::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::ColorEdit4("低速カラー", &slowColor_.x);
    ImGui::ColorEdit4("高速カラー", &fastColor_.x);
    ImGui::DragFloat("低速閾値",    &slowSpeed_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("高速閾値",    &fastSpeed_, 0.1f, 0.0f, 100.0f);
#endif
}

void UpdateColorBySpeed::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"slowColor",{slowColor_.x,slowColor_.y,slowColor_.z,slowColor_.w}},
        {"fastColor",{fastColor_.x,fastColor_.y,fastColor_.z,fastColor_.w}},
        {"slowSpeed",slowSpeed_},{"fastSpeed",fastSpeed_}
    };
}

void UpdateColorBySpeed::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("slowColor")) { auto c=json["slowColor"]; slowColor_={c[0],c[1],c[2],c[3]}; }
    if (json.contains("fastColor")) { auto c=json["fastColor"]; fastColor_={c[0],c[1],c[2],c[3]}; }
    if (json.contains("slowSpeed")) slowSpeed_ = json["slowSpeed"];
    if (json.contains("fastSpeed")) fastSpeed_ = json["fastSpeed"];
}
