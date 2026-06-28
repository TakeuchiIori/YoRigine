#include "UpdateCollision.h"

void UpdateCollision::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    (void)dt;
    
    // 地面より下にある場合
    if (attrs[index].position.y < groundHeight_) {
        if (killOnCollision_) {
            // 衝突で消滅
            attrs[index].isActive = false;
        } else {
            // バウンド処理
            attrs[index].position.y = groundHeight_;
            attrs[index].velocity.y = -attrs[index].velocity.y * bounciness_;
            
            // 地面との摩擦（水平方向の速度を減衰）
            attrs[index].velocity.x *= 0.9f;
            attrs[index].velocity.z *= 0.9f;
        }
    }
}

void UpdateCollision::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("地面の高さ", &groundHeight_, 0.1f, -100.0f, 100.0f);
    ImGui::DragFloat("反発係数", &bounciness_, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("衝突時に消滅", &killOnCollision_);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "水平面との衝突を処理");
#endif
}

void UpdateCollision::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"groundHeight", groundHeight_},
        {"bounciness", bounciness_},
        {"killOnCollision", killOnCollision_}
    };
}

void UpdateCollision::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("groundHeight")) groundHeight_ = json["groundHeight"];
    if (json.contains("bounciness")) bounciness_ = json["bounciness"];
    if (json.contains("killOnCollision")) killOnCollision_ = json["killOnCollision"];
}
