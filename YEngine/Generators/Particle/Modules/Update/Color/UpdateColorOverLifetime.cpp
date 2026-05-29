#include "UpdateColorOverLifetime.h"
#include <algorithm>

UpdateColorOverLifetime::UpdateColorOverLifetime()
{
    // デフォルト: 炎グラデーション
    times_  = { 0.0f, 0.3f, 0.7f, 1.0f, 1.0f };
    colors_ = {
        Vector4{1.0f,1.0f,0.8f,1.0f},  // 白黄
        Vector4{1.0f,0.5f,0.0f,1.0f},  // オレンジ
        Vector4{0.8f,0.1f,0.0f,0.5f},  // 赤（半透明）
        Vector4{0.2f,0.0f,0.0f,0.0f},  // 暗赤（透明）
        Vector4{0.0f,0.0f,0.0f,0.0f}
    };
}

void UpdateColorOverLifetime::OnUpdate(ParticleAttribute* attrs, uint32_t index, float)
{
    float t = attrs[index].GetNormalizedAge();
    if (stopCount_ <= 1) { attrs[index].color = colors_[0]; return; }

    // t が属する区間を探す
    for (int i = 0; i < stopCount_ - 1; ++i) {
        float t0 = times_[i], t1 = times_[i+1];
        if (t >= t0 && t <= t1) {
            float local = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
            auto& c0 = colors_[i];
            auto& c1 = colors_[i+1];
            attrs[index].color = {
                c0.x + (c1.x - c0.x) * local,
                c0.y + (c1.y - c0.y) * local,
                c0.z + (c1.z - c0.z) * local,
                c0.w + (c1.w - c0.w) * local
            };
            return;
        }
    }
    attrs[index].color = colors_[stopCount_ - 1];
}

void UpdateColorOverLifetime::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragInt("ストップ数", &stopCount_, 1, 2, kMaxStops);
    for (int i = 0; i < stopCount_; ++i) {
        char buf[32];
        sprintf_s(buf, "時刻 %d", i);
        ImGui::SliderFloat(buf, &times_[i], 0.0f, 1.0f);
        sprintf_s(buf, "カラー %d", i);
        ImGui::ColorEdit4(buf, &colors_[i].x);
    }
#endif
}

void UpdateColorOverLifetime::SaveToJson(nlohmann::json& json) const
{
    nlohmann::json stops = nlohmann::json::array();
    for (int i = 0; i < stopCount_; ++i) {
        stops.push_back({
            {"t",     times_[i]},
            {"color", {colors_[i].x,colors_[i].y,colors_[i].z,colors_[i].w}}
        });
    }
    json = { {"stops",stops} };
}

void UpdateColorOverLifetime::LoadFromJson(const nlohmann::json& json)
{
    if (!json.contains("stops")) return;
    auto& stops = json["stops"];
    stopCount_ = std::min(static_cast<int>(stops.size()), kMaxStops);
    for (int i = 0; i < stopCount_; ++i) {
        times_[i]  = stops[i]["t"];
        auto c     = stops[i]["color"];
        colors_[i] = { c[0],c[1],c[2],c[3] };
    }
}
