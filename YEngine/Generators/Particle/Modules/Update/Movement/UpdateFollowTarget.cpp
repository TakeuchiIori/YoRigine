#include "UpdateFollowTarget.h"
#include <cmath>
#include <algorithm>

void UpdateFollowTarget::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    auto& a = attrs[index];
    float dx = targetPos_.x - a.position.x;
    float dy = targetPos_.y - a.position.y;
    float dz = targetPos_.z - a.position.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (dist < 1e-4f) return;

    float invD = 1.0f / dist;
    a.velocity.x += dx * invD * acceleration_ * dt;
    a.velocity.y += dy * invD * acceleration_ * dt;
    a.velocity.z += dz * invD * acceleration_ * dt;

    // 速度クランプ
    float spd = a.GetSpeed();
    if (spd > maxSpeed_) {
        float scale = maxSpeed_ / spd;
        a.velocity.x *= scale;
        a.velocity.y *= scale;
        a.velocity.z *= scale;
    }
}

void UpdateFollowTarget::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("ターゲット位置", &targetPos_.x,  0.1f);
    ImGui::DragFloat("加速度",          &acceleration_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("最大速度",        &maxSpeed_,     0.1f, 0.0f, 100.0f);
    ImGui::TextDisabled("ゲーム側から SetTargetPos() で更新してください");
#endif
}

void UpdateFollowTarget::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"targetPos",{targetPos_.x,targetPos_.y,targetPos_.z}},
        {"acceleration",acceleration_},{"maxSpeed",maxSpeed_}
    };
}

void UpdateFollowTarget::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("targetPos"))   { auto v=json["targetPos"]; targetPos_={v[0],v[1],v[2]}; }
    if (json.contains("acceleration")) acceleration_ = json["acceleration"];
    if (json.contains("maxSpeed"))     maxSpeed_     = json["maxSpeed"];
}
