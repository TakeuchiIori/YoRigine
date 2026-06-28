#include "UpdateColorGradient.h"

void UpdateColorGradient::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    (void)dt;
    
    float t = attrs[index].GetNormalizedAge();
    
    // tを色の区間に変換
    float scaledT = t * (colorCount_ - 1);
    int colorIndex = static_cast<int>(scaledT);
    
    // 範囲外チェック
    if (colorIndex >= colorCount_ - 1) {
        attrs[index].color = colors_[colorCount_ - 1];
        return;
    }
    
    // 区間内での補間値を計算
    float localT = scaledT - colorIndex;
    
    // 2つの色の間で補間
    attrs[index].color = Lerp(colors_[colorIndex], colors_[colorIndex + 1], localT);
}

void UpdateColorGradient::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::SliderInt("色の数", &colorCount_, 2, MAX_COLORS);

    for (int i = 0; i < colorCount_; ++i) {
        ImGui::PushID(i);
        char label[32];
        snprintf(label, sizeof(label), "色 %d", i);
        ImGui::ColorEdit4(label, &colors_[i].x);
        ImGui::PopID();
    }

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "寿命に応じた多色グラデーション");
#endif
}

void UpdateColorGradient::SaveToJson(nlohmann::json& json) const
{
    json["colorCount"] = colorCount_;
    nlohmann::json colorsArray = nlohmann::json::array();
    
    for (int i = 0; i < colorCount_; ++i) {
        colorsArray.push_back({
            {"x", colors_[i].x},
            {"y", colors_[i].y},
            {"z", colors_[i].z},
            {"w", colors_[i].w}
        });
    }
    json["colors"] = colorsArray;
}

void UpdateColorGradient::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("colorCount")) {
        colorCount_ = json["colorCount"];
    }
    
    if (json.contains("colors") && json["colors"].is_array()) {
        auto colorsArray = json["colors"];
        for (size_t i = 0; i < colorsArray.size() && i < MAX_COLORS; ++i) {
            auto& c = colorsArray[i];
            colors_[i] = {
                c["x"].get<float>(),
                c["y"].get<float>(),
                c["z"].get<float>(),
                c["w"].get<float>()
            };
        }
    }
}
