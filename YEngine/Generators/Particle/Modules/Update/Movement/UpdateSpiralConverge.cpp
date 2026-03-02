#include "UpdateSpiralConverge.h"
#include "MathFunc.h"

void UpdateSpiralConverge::Initialize(uint32_t maxParticles)
{
    particleAngle_.resize(maxParticles);
    for (uint32_t i = 0; i < maxParticles; ++i) {
        particleAngle_[i] = ParticleMath::RandomRange(0.0f, 2.0f * 3.14159265f);
    }
}

void UpdateSpiralConverge::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    // ターゲットまでの方向と距離
    Vector3 toTarget = targetPoint_ - attrs[index].position;
    float distance = Length(toTarget);
    
    if (distance > 0.1f) {
        // 角度を更新
        particleAngle_[index] += spiralSpeed_ * dt;
        
        // 時間経過で半径を縮小（収束効果）
        float currentRadius = spiralRadius_ * (1.0f - attrs[index].GetNormalizedAge());
        
        // スパイラル軌道の計算
        Vector3 spiralOffset = {
            cosf(particleAngle_[index]) * currentRadius,
            0.0f,
            sinf(particleAngle_[index]) * currentRadius
        };
        
        // ターゲット方向への移動
        Vector3 convergeVelocity = Normalize(toTarget) * convergeSpeed_;
        
        // スパイラルオフセットを適用しながら収束
        attrs[index].velocity = convergeVelocity;
        attrs[index].position = attrs[index].position + spiralOffset * dt;
    }
}

void UpdateSpiralConverge::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragFloat3("目標点", &targetPoint_.x, 0.1f);
    ImGui::DragFloat("螺旋速度", &spiralSpeed_, 0.1f, 0.0f, 10.0f);
    ImGui::DragFloat("収束速度", &convergeSpeed_, 0.1f, 0.0f, 20.0f);
    ImGui::DragFloat("螺旋半径", &spiralRadius_, 0.1f, 0.0f, 10.0f);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "チャージ・集束エフェクトに最適");
#endif
}

void UpdateSpiralConverge::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"targetPoint", {targetPoint_.x, targetPoint_.y, targetPoint_.z}},
        {"spiralSpeed", spiralSpeed_},
        {"convergeSpeed", convergeSpeed_},
        {"spiralRadius", spiralRadius_}
    };
}

void UpdateSpiralConverge::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("targetPoint")) {
        auto t = json["targetPoint"];
        targetPoint_ = { t[0], t[1], t[2] };
    }
    if (json.contains("spiralSpeed")) spiralSpeed_ = json["spiralSpeed"];
    if (json.contains("convergeSpeed")) convergeSpeed_ = json["convergeSpeed"];
    if (json.contains("spiralRadius")) spiralRadius_ = json["spiralRadius"];
}
