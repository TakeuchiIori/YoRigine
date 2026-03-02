#include "SpawnConeVelocity.h"
#include "../../ParticleAttribute.h"

void SpawnConeVelocity::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    // 度数法からラジアンに変換
    float angleRad = coneAngle_ * 3.14159265f / 180.0f;
    
    // ランダムな角度でコーン内の方向を生成
    float theta = ParticleMath::RandomRange(0.0f, 2.0f * 3.14159265f);
    float phi = ParticleMath::RandomRange(0.0f, angleRad);
    
    // 球面座標から直交座標へ変換
    float sinPhi = sinf(phi);
    Vector3 randomDir = {
        sinPhi * cosf(theta),
        cosf(phi),
        sinPhi * sinf(theta)
    };
    
    // direction_ 方向へ回転（簡易版：Y軸方向と仮定）
    Vector3 normalizedDir = Normalize(direction_);
    Vector3 velocity = randomDir;
    
    // 速度にランダム性を加える
    float finalSpeed = speed_ + ParticleMath::RandomRange(-speedVariation_, speedVariation_);
    
    attrs[index].velocity = velocity * finalSpeed;
}

void SpawnConeVelocity::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("方向", &direction_.x, 0.1f);
    ImGui::DragFloat("角度", &coneAngle_, 1.0f, 0.0f, 180.0f);
    ImGui::DragFloat("速さ", &speed_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("バリエーション", &speedVariation_, 0.1f, 0.0f, 50.0f);
#endif
}

void SpawnConeVelocity::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"direction", {direction_.x, direction_.y, direction_.z}},
        {"coneAngle", coneAngle_},
        {"speed", speed_},
        {"speedVariation", speedVariation_}
    };
}

void SpawnConeVelocity::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("direction")) {
        auto dir = json["direction"];
        direction_ = { dir[0], dir[1], dir[2] };
    }
    if (json.contains("coneAngle")) coneAngle_ = json["coneAngle"];
    if (json.contains("speed")) speed_ = json["speed"];
    if (json.contains("speedVariation")) speedVariation_ = json["speedVariation"];
}
