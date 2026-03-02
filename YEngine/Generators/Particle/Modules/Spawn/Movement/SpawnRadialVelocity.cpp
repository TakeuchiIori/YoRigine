#include "SpawnRadialVelocity.h"
#include "../../ParticleAttribute.h"
#include "MathFunc.h"

void SpawnRadialVelocity::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    Vector3 direction;
    
    if (use3D_) {
        // 3D球面上のランダムな方向
        direction = Normalize(ParticleMath::RandomInSphere(1.0f));
        // 正規化されていない場合に備えて再正規化
        if (Length(direction) < 0.001f) {
            direction = { 0.0f, 1.0f, 0.0f };
        }
    } else {
        // XZ平面上のランダムな方向
        float angle = ParticleMath::RandomRange(0.0f, 2.0f * 3.14159265f);
        direction = {
            cosf(angle),
            0.0f,
            sinf(angle)
        };
    }
    
    float finalSpeed = speed_ + ParticleMath::RandomRange(-speedVariation_, speedVariation_);
    attrs[index].velocity = direction * finalSpeed;
}

void SpawnRadialVelocity::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat("速さ", &speed_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("バリエーション", &speedVariation_, 0.1f, 0.0f, 50.0f);
    ImGui::Checkbox("3D", &use3D_);
    if (!use3D_) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(XZ plane only)");
    }
#endif
}

void SpawnRadialVelocity::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"speed", speed_},
        {"speedVariation", speedVariation_},
        {"use3D", use3D_}
    };
}

void SpawnRadialVelocity::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("speed")) speed_ = json["speed"];
    if (json.contains("speedVariation")) speedVariation_ = json["speedVariation"];
    if (json.contains("use3D")) use3D_ = json["use3D"];
}
