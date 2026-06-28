#include "UpdateStretchByVelocity.h"
#include <cmath>
#include <algorithm>

void UpdateStretchByVelocity::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    auto& a = attrs[index];
    float speed = a.GetSpeed();
    float stretch = std::min(speed * stretchFactor_, maxStretch_);

    // Y 軸を速度方向に伸ばす（ビルボードを使わない場合想定）
    a.scale.y = a.initialScale.y * (1.0f + stretch);

    // rotation.x を速度のピッチに合わせる
    if (speed > 1e-4f) {
        float pitch = std::atan2(-a.velocity.y, std::sqrt(a.velocity.x*a.velocity.x + a.velocity.z*a.velocity.z));
        float yaw   = std::atan2(a.velocity.x, a.velocity.z);
        a.rotation.x = pitch;
        a.rotation.y = yaw;
    }
}

void UpdateStretchByVelocity::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("引き伸ばし係数", &stretchFactor_, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("最大倍率",       &maxStretch_,    0.1f,  1.0f, 20.0f);
    ImGui::TextDisabled("速度が大きいほど Y スケールが伸びます");
    ImGui::TextDisabled("ビルボードタイプ = None 推奨");
#endif
}

void UpdateStretchByVelocity::SaveToJson(nlohmann::json& json) const
{
    json = { {"stretchFactor",stretchFactor_},{"maxStretch",maxStretch_} };
}

void UpdateStretchByVelocity::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("stretchFactor")) stretchFactor_ = json["stretchFactor"];
    if (json.contains("maxStretch"))    maxStretch_    = json["maxStretch"];
}
