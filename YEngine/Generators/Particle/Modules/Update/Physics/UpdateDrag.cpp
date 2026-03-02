#include "UpdateDrag.h"

void UpdateDrag::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    // 速度に抵抗を適用（指数減衰）
    float dragFactor = 1.0f - dragCoefficient_ * dt;
    if (dragFactor < 0.0f) dragFactor = 0.0f;
    
    attrs[index].velocity = attrs[index].velocity * dragFactor;
}

void UpdateDrag::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("抵抗係数", &dragCoefficient_, 0.01f, 0.0f, 10.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "値が大きいほど減速が速い");
#endif
}

void UpdateDrag::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"dragCoefficient", dragCoefficient_}
    };
}

void UpdateDrag::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("dragCoefficient")) {
        dragCoefficient_ = json["dragCoefficient"];
    }
}
