#include "SpawnRandomRotate.h"
#include "../../ParticleAttribute.h"

void SpawnRandomRotate::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    Vector3 result;

    // X軸
    result.x = randomX_ ? ParticleMath::RandomRange(0.0f, 360.0f) : baseRotation_.x;
    // Y軸
    result.y = randomY_ ? ParticleMath::RandomRange(0.0f, 360.0f) : baseRotation_.y;
    // Z軸
    result.z = randomZ_ ? ParticleMath::RandomRange(0.0f, 360.0f) : baseRotation_.z;

    // 度からラジアンに変換して代入
    attrs[index].rotation = {
        result.x * kDegToRad,
        result.y * kDegToRad,
        result.z * kDegToRad
    };
}

void SpawnRandomRotate::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::Text("Rotate Settings (Degrees)");

    // X軸の設定
    ImGui::Checkbox("Random X", &randomX_);
    if (!randomX_) {
        ImGui::SameLine();
        ImGui::DragFloat("##X", &baseRotation_.x, 1.0f, 0.0f, 360.0f);
    }

    // Y軸の設定
    ImGui::Checkbox("Random Y", &randomY_);
    if (!randomY_) {
        ImGui::SameLine();
        ImGui::DragFloat("##Y", &baseRotation_.y, 1.0f, 0.0f, 360.0f);
    }

    // Z軸の設定
    ImGui::Checkbox("Random Z", &randomZ_);
    if (!randomZ_) {
        ImGui::SameLine();
        ImGui::DragFloat("##Z", &baseRotation_.z, 1.0f, 0.0f, 360.0f);
    }
#endif
}

void SpawnRandomRotate::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"randomX", randomX_},
        {"randomY", randomY_},
        {"randomZ", randomZ_},
        {"baseRotation", {{"x", baseRotation_.x}, {"y", baseRotation_.y}, {"z", baseRotation_.z}}}
    };
}

void SpawnRandomRotate::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("randomX")) randomX_ = json["randomX"];
    if (json.contains("randomY")) randomY_ = json["randomY"];
    if (json.contains("randomZ")) randomZ_ = json["randomZ"];
    if (json.contains("baseRotation")) {
        baseRotation_.x = json["baseRotation"]["x"];
        baseRotation_.y = json["baseRotation"]["y"];
        baseRotation_.z = json["baseRotation"]["z"];
    }
}