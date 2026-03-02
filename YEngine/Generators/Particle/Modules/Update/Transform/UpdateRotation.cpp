#include "UpdateRotation.h"


void UpdateRotation::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    // 回転角度を更新（度数法）
    attrs[index].rotation.x += rotationSpeed_.x * dt;
    attrs[index].rotation.y += rotationSpeed_.y * dt;
    attrs[index].rotation.z += rotationSpeed_.z * dt;
    
    // 注: 実際の回転適用はレンダラー側で行う
    // ここでは回転情報を保持するのみ
    // ParticleAttributeに回転情報がある場合はそこに格納
}

void UpdateRotation::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("回転速度 (deg/s)", &rotationSpeed_.x, 1.0f, -360.0f, 360.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "注: 回転機能にはレンダラー側の対応が必要です");
#endif
}

void UpdateRotation::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"rotationSpeed", {rotationSpeed_.x, rotationSpeed_.y, rotationSpeed_.z}}
    };
}

void UpdateRotation::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("rotationSpeed")) {
        auto r = json["rotationSpeed"];
        rotationSpeed_ = { r[0], r[1], r[2] };
    }
}
