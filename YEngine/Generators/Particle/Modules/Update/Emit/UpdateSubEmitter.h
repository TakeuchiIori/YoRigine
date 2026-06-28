#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include <string>

/// <summary>
/// サブエミッタ：ある粒（親）から別の YParticleSystem（子）を発生させる。
///
/// ■ トリガ
///   OnDeath    : 親が寿命で消える瞬間に子を出す（火花→小煙、弾→着弾爆発 など）
///   Continuous : 親が生きている間、一定レートで子を撒く（飛翔体の煙トレイル など）
///
/// レイヤリング（安いパーツを重ねる）で複合エフェクトを組むための土台。
/// 子の発生位置は親粒の position（ワールド空間運用が前提）。
/// 暴走防止：子の同時数は子システムの maxParticles で頭打ちになる。
/// </summary>
class UpdateSubEmitter : public IUpdateModule {
public:
    enum class Trigger : uint8_t {
        OnDeath = 0,     // 死亡時に1回
        Continuous = 1   // 生存中に継続
    };

    //===== IUpdateModule =====
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void OnDeath(ParticleAttribute* attrs, uint32_t index)           override;
    void DrawEditor()                                                 override;
    std::string GetName()     const override { return "サブエミッタ"; }
    std::string GetTypeName() const override { return "UpdateSubEmitter"; }

    //===== シリアライズ =====
    void SaveToJson(nlohmann::json& json)        const override;
    void LoadFromJson(const nlohmann::json& json)      override;

    //===== アクセス =====
    void SetChildSystem(const std::string& name) { childSystem_ = name; }
    const std::string& GetChildSystem() const { return childSystem_; }
    void SetTrigger(Trigger t) { trigger_ = t; }
    Trigger GetTrigger() const { return trigger_; }

private:
    /// 子システムを発生させる（子システム未登録なら何もしない）
    void EmitChild(const Vector3& position);

    std::string childSystem_;             // 発生させる子システム名
    Trigger     trigger_   = Trigger::OnDeath;
    int         count_     = 8;           // 1回の発生数
    float       rate_      = 20.0f;       // Continuous: 親1粒あたりの秒間発生数
    float       probability_ = 1.0f;      // 発生確率（0〜1）。OnDeath で抽選にも使える
};

REGISTER_UPDATE_MODULE(UpdateSubEmitter)
