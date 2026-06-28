#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

/// <summary>
/// ターゲット位置（ワールド座標）に向かって加速するホーミング
/// TargetPos をゲーム側から SetTargetPos() で更新する
/// 用途: ホーミング弾、引き寄せエフェクト、ロックオン
/// </summary>
class UpdateFollowTarget : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "ターゲット追従"; }
    std::string GetTypeName() const override { return "UpdateFollowTarget"; }
    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

    void SetTargetPos(const Vector3& pos) { targetPos_ = pos; }
    const Vector3& GetTargetPos() const   { return targetPos_; }

private:
    Vector3 targetPos_    = { 0.0f, 0.0f, 0.0f };
    float   acceleration_ = 10.0f; // 追従加速度
    float   maxSpeed_     = 15.0f;
};
REGISTER_UPDATE_MODULE(UpdateFollowTarget)
