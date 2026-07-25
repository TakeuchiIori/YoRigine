#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"

#include "Vector2.h"

/// <summary>
/// UV ちらつき（フリッカー）
///
/// ジグザグ／炎テクスチャの UV を主に上下(V)方向へ「まばらに」揺らして
/// めらめらとしたゆらめきを作るモジュール。
/// 定速の UpdateUVScroll と違い、粒ごとに位相をずらし、2 つの周波数を
/// 重ね合わせることで不規則（＝まばら）な往復運動になる。
///
/// uvOffset を「ドリフト成分 + 揺れ成分」で毎フレーム上書きするため、
/// このモジュールが UV の主駆動になる。UpdateUVScroll と併用すると打ち消し合う。
/// ゆっくり流したい場合は driftSpeed を使うこと。
///
/// 粒ごとのばらつきは ParticleAttribute::phase を優先し、
/// phase==0（SpawnPhase 未使用）のときは粒インデックスから安定した擬似位相を作る。
/// </summary>
class UpdateUVFlicker : public IUpdateModule {
public:
    void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void DrawEditor() override;
    std::string GetName()     const override { return "UVちらつき"; }
    std::string GetTypeName() const override { return "UpdateUVFlicker"; }

    void SaveToJson(nlohmann::json& json) const override;
    void LoadFromJson(const nlohmann::json& json) override;

private:
    Vector2 amplitude_    = { 0.02f, 0.15f }; // 揺れ幅(UV)。X は控えめ・Y(上下)が主
    Vector2 frequency_    = { 3.0f,  5.0f };  // 基本周波数(Hz)
    Vector2 driftSpeed_   = { 0.0f,  0.0f };  // 定常スクロール成分(任意/秒)
    float   irregularity_ = 0.6f;             // 2 つ目の周波数の混ざり具合(まばらさ) [0,1]
};
