#include "SpawnDirection.h"
#include <cmath>

static constexpr float kPi = 3.14159265f;

void SpawnDirection::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    // 基準方向を正規化
    float len = std::sqrt(direction_.x*direction_.x +
                          direction_.y*direction_.y +
                          direction_.z*direction_.z);
    Vector3 d = (len > 1e-5f)
        ? Vector3{ direction_.x/len, direction_.y/len, direction_.z/len }
        : Vector3{ 0,1,0 };

    // ランダムな直交系を構築してコーン内にランダム方向を生成
    float halfRad = spreadAngle_ * kPi / 180.0f;
    float theta   = ParticleMath::RandomRange(0.0f, halfRad);
    float phi     = ParticleMath::RandomRange(0.0f, 2.0f * kPi);

    // ローカルコーン方向
    float sinT = std::sin(theta), cosT = std::cos(theta);
    float sx = sinT * std::cos(phi), sy = sinT * std::sin(phi), sz = cosT;

    // d を Z 軸とした回転行列で変換
    Vector3 up = (std::abs(d.y) < 0.99f) ? Vector3{0,1,0} : Vector3{1,0,0};
    // right = cross(d, up) normalized
    Vector3 right = {
        d.y*up.z - d.z*up.y,
        d.z*up.x - d.x*up.z,
        d.x*up.y - d.y*up.x
    };
    float rl = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
    right = { right.x/rl, right.y/rl, right.z/rl };
    // forward = cross(right, d)
    Vector3 fwd = {
        right.y*d.z - right.z*d.y,
        right.z*d.x - right.x*d.z,
        right.x*d.y - right.y*d.x
    };

    Vector3 dir = {
        right.x*sx + fwd.x*sy + d.x*sz,
        right.y*sx + fwd.y*sy + d.y*sz,
        right.z*sx + fwd.z*sy + d.z*sz
    };

    float speed = ParticleMath::RandomRange(minSpeed_, maxSpeed_);
    attrs[index].velocity = { dir.x*speed, dir.y*speed, dir.z*speed };
}

void SpawnDirection::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("方向",       &direction_.x,  0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("拡散角 (°)", &spreadAngle_,  0.5f,  0.0f, 180.0f);
    ImGui::DragFloat("最小速度",    &minSpeed_,     0.1f,  0.0f, 100.0f);
    ImGui::DragFloat("最大速度",    &maxSpeed_,     0.1f,  0.0f, 100.0f);
#endif
}

void SpawnDirection::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"direction",   {direction_.x, direction_.y, direction_.z}},
        {"spreadAngle", spreadAngle_},
        {"minSpeed",    minSpeed_},
        {"maxSpeed",    maxSpeed_}
    };
}

void SpawnDirection::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("direction")) {
        auto d = json["direction"];
        direction_ = { d[0], d[1], d[2] };
    }
    if (json.contains("spreadAngle")) spreadAngle_ = json["spreadAngle"];
    if (json.contains("minSpeed"))    minSpeed_    = json["minSpeed"];
    if (json.contains("maxSpeed"))    maxSpeed_    = json["maxSpeed"];
}
