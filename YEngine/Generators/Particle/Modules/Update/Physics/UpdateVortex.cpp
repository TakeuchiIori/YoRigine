#include "UpdateVortex.h"
#include <cmath>
static constexpr float kDeg2Rad = 3.14159265f / 180.0f;

void UpdateVortex::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    auto& a = attrs[index];

    // 中心からのベクトル（軸に垂直な成分のみ）
    float dx = a.position.x - center_.x;
    float dy = a.position.y - center_.y;
    float dz = a.position.z - center_.z;

    // 軸を正規化
    float al = std::sqrt(axis_.x*axis_.x + axis_.y*axis_.y + axis_.z*axis_.z);
    if (al < 1e-5f) return;
    Vector3 ax = { axis_.x/al, axis_.y/al, axis_.z/al };

    // 軸方向の成分を除いた半径ベクトル
    float proj = dx*ax.x + dy*ax.y + dz*ax.z;
    float rx = dx - ax.x*proj;
    float ry = dy - ax.y*proj;
    float rz = dz - ax.z*proj;
    float r  = std::sqrt(rx*rx + ry*ry + rz*rz);
    if (r < 1e-5f) return;

    // 接線方向 = cross(axis, r/|r|)
    float rnx = rx/r, rny = ry/r, rnz = rz/r;
    float tx = ax.y*rnz - ax.z*rny;
    float ty = ax.z*rnx - ax.x*rnz;
    float tz = ax.x*rny - ax.y*rnx;

    float w = angularSpeed_ * kDeg2Rad;
    a.velocity.x += tx * w * r * dt - rx * attractForce_ * dt;
    a.velocity.y += ty * w * r * dt - ry * attractForce_ * dt;
    a.velocity.z += tz * w * r * dt - rz * attractForce_ * dt;
}

void UpdateVortex::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("中心位置",   &center_.x,       0.1f);
    ImGui::DragFloat3("回転軸",     &axis_.x,         0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("角速度 (°/s)",&angularSpeed_,   1.0f);
    ImGui::DragFloat("引力",        &attractForce_,   0.01f, 0.0f, 10.0f);
#endif
}

void UpdateVortex::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"center",{center_.x,center_.y,center_.z}},
        {"axis",{axis_.x,axis_.y,axis_.z}},
        {"angularSpeed",angularSpeed_},
        {"attractForce",attractForce_}
    };
}

void UpdateVortex::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("center"))       { auto v=json["center"]; center_={v[0],v[1],v[2]}; }
    if (json.contains("axis"))         { auto v=json["axis"];   axis_  ={v[0],v[1],v[2]}; }
    if (json.contains("angularSpeed")) angularSpeed_ = json["angularSpeed"];
    if (json.contains("attractForce")) attractForce_ = json["attractForce"];
}
