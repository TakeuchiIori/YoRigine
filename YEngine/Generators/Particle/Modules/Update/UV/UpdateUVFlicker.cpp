#include "UpdateUVFlicker.h"

#include <cmath>

namespace {
    constexpr float kTwoPi = 6.28318530718f;

    // 粒インデックスから [0, 2π) の安定した擬似位相を作る（SpawnPhase 不要でもばらつく）
    float PhaseFromIndex(uint32_t index) {
        uint32_t h = index * 2654435761u; // Knuth の乗算ハッシュ
        h ^= h >> 15;
        return (h & 0xffffu) / 65535.0f * kTwoPi;
    }
}

void UpdateUVFlicker::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) {
    (void)dt; // 時間は currentTime を直接使うので dt は未使用
    ParticleAttribute& p = attrs[index];

    // 粒ごとの位相。SpawnPhase があればそれを、無ければインデックスから生成
    const float seed = (p.phase != 0.0f) ? p.phase : PhaseFromIndex(index);

    const float t = p.currentTime;

    // 2 周波の重ね合わせで不規則（まばら）な上下ゆらぎを作る
    float wy = std::sin(t * frequency_.y * kTwoPi + seed)
             + irregularity_ * std::sin(t * frequency_.y * 2.37f * kTwoPi + seed * 1.7f);
    wy /= (1.0f + irregularity_);

    // 横は控えめに一周波だけ
    const float wx = std::sin(t * frequency_.x * kTwoPi + seed * 0.5f);

    // ドリフト（定常スクロール）＋ 揺れ成分で uvOffset を上書き
    p.uvOffset.x = driftSpeed_.x * t + wx * amplitude_.x;
    p.uvOffset.y = driftSpeed_.y * t + wy * amplitude_.y;
}

void UpdateUVFlicker::DrawEditor() {
#ifdef USE_IMGUI
    ImGui::DragFloat2("揺れ幅(UV) X/Y",   &amplitude_.x,  0.005f, 0.0f, 1.0f);
    ImGui::DragFloat2("周波数(Hz) X/Y",   &frequency_.x,  0.05f,  0.0f, 60.0f);
    ImGui::DragFloat2("ドリフト速度 X/Y", &driftSpeed_.x, 0.01f);
    ImGui::DragFloat("まばらさ",          &irregularity_, 0.01f,  0.0f, 1.0f);
#endif
}

void UpdateUVFlicker::SaveToJson(nlohmann::json& json) const {
    json = {
        { "amplitudeX",    amplitude_.x },
        { "amplitudeY",    amplitude_.y },
        { "frequencyX",    frequency_.x },
        { "frequencyY",    frequency_.y },
        { "driftSpeedX",   driftSpeed_.x },
        { "driftSpeedY",   driftSpeed_.y },
        { "irregularity",  irregularity_ },
    };
}

void UpdateUVFlicker::LoadFromJson(const nlohmann::json& json) {
    if (json.contains("amplitudeX"))   amplitude_.x   = json["amplitudeX"];
    if (json.contains("amplitudeY"))   amplitude_.y   = json["amplitudeY"];
    if (json.contains("frequencyX"))   frequency_.x   = json["frequencyX"];
    if (json.contains("frequencyY"))   frequency_.y   = json["frequencyY"];
    if (json.contains("driftSpeedX"))  driftSpeed_.x  = json["driftSpeedX"];
    if (json.contains("driftSpeedY"))  driftSpeed_.y  = json["driftSpeedY"];
    if (json.contains("irregularity")) irregularity_  = json["irregularity"];
}

// JSON ロード用ファクトリ登録（.cpp 側で 1 度だけ登録）
REGISTER_UPDATE_MODULE(UpdateUVFlicker)
