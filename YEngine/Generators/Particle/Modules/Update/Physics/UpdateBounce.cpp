#include "UpdateBounce.h"
#include <cmath>

void UpdateBounce::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    auto& a = attrs[index];
    if (a.position.y <= floorY_ && a.velocity.y < 0.0f) {
        if (std::abs(a.velocity.y) > minBounceSpeed_) {
            a.velocity.y = -a.velocity.y * restitution_;
        } else {
            a.velocity.y = 0.0f;
        }
        // 水平摩擦
        a.velocity.x *= (1.0f - friction_);
        a.velocity.z *= (1.0f - friction_);
        // 床の上に補正
        a.position.y = floorY_;
    }
}

void UpdateBounce::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("床の Y",       &floorY_,         0.1f);
    ImGui::DragFloat("跳ね返り係数", &restitution_,    0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("水平摩擦",     &friction_,       0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("最小バウンス速度", &minBounceSpeed_, 0.01f, 0.0f, 5.0f);
    ImGui::TextDisabled("UpdateGravity と組み合わせて使用");
#endif
}

void UpdateBounce::SaveToJson(nlohmann::json& json) const
{
    json = { {"floorY",floorY_},{"restitution",restitution_},
             {"friction",friction_},{"minBounceSpeed",minBounceSpeed_} };
}

void UpdateBounce::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("floorY"))         floorY_         = json["floorY"];
    if (json.contains("restitution"))    restitution_    = json["restitution"];
    if (json.contains("friction"))       friction_       = json["friction"];
    if (json.contains("minBounceSpeed")) minBounceSpeed_ = json["minBounceSpeed"];
}
