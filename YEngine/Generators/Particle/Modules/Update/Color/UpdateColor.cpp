#include "UpdateColor.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include <ImCurveEdit.h>
#endif

//=============================================================================
// 構築
//=============================================================================

UpdateColor::UpdateColor() {
    // Alpha のデフォルトカーブ: 1.0 → 0.0 (フェードアウト)
    curveA_.GetChannel().Clear();
    curveA_.GetChannel().AddKey(0.0f, 1.0f);
    curveA_.GetChannel().AddKey(1.0f, 0.0f);
    curveA_.SetMode(CurveProperty::Mode::Curve);
}

void UpdateColor::SetColorMode(ColorMode m) {
    colorMode_ = m;
#ifdef USE_IMGUI
    delegateDirty_ = true;
#endif
}

//=============================================================================
// OnUpdate
//=============================================================================

void UpdateColor::OnUpdate(ParticleAttribute* attrs, uint32_t index, float /*dt*/) {
    const float t = attrs[index].GetNormalizedAge(); // 0.0〜1.0
    const float rand = 0.5f; // ★ per-particle ランダムを使う場合は ParticleAttribute に持たせる

    switch (colorMode_) {
    case ColorMode::TwoColor:
        attrs[index].color = Lerp(startColor_, endColor_, t);
        break;

    case ColorMode::GradientCurve:
        attrs[index].color = {
            curveR_.Evaluate(t, rand),
            curveG_.Evaluate(t, rand),
            curveB_.Evaluate(t, rand),
            curveA_.Evaluate(t, rand)
        };
        break;
    }
}

//=============================================================================
// JSON シリアライズ
//=============================================================================

void UpdateColor::SaveToJson(nlohmann::json& json) const {
    json["colorMode"] = static_cast<int>(colorMode_);
    json["startColor"] = { startColor_.x, startColor_.y, startColor_.z, startColor_.w };
    json["endColor"] = { endColor_.x,   endColor_.y,   endColor_.z,   endColor_.w };
    json["curveR"] = curveR_.SaveToJson();
    json["curveG"] = curveG_.SaveToJson();
    json["curveB"] = curveB_.SaveToJson();
    json["curveA"] = curveA_.SaveToJson();
}

void UpdateColor::LoadFromJson(const nlohmann::json& json) {
    colorMode_ = static_cast<ColorMode>(json.value("colorMode", 0));
#ifdef USE_IMGUI
    delegateDirty_ = true;
#endif
    if (json.contains("startColor"))
        startColor_ = { json["startColor"][0], json["startColor"][1],
                        json["startColor"][2], json["startColor"][3] };
    if (json.contains("endColor"))
        endColor_ = { json["endColor"][0], json["endColor"][1],
                      json["endColor"][2], json["endColor"][3] };

    if (json.contains("curveR")) curveR_.LoadFromJson(json["curveR"]);
    if (json.contains("curveG")) curveG_.LoadFromJson(json["curveG"]);
    if (json.contains("curveB")) curveB_.LoadFromJson(json["curveB"]);
    if (json.contains("curveA")) curveA_.LoadFromJson(json["curveA"]);
}

//=============================================================================
// DrawEditor
//=============================================================================
#ifdef USE_IMGUI

void UpdateColor::DrawEditor() {
    // カラーモードセレクタ
    static const char* kModeNames[] = { "2色補間", "カーブ (R/G/B/A)" };
    int modeIdx = static_cast<int>(colorMode_);
    if (ImGui::Combo("カラーモード", &modeIdx, kModeNames, 2))
        SetColorMode(static_cast<ColorMode>(modeIdx));

    ImGui::Separator();

    switch (colorMode_) {
    case ColorMode::TwoColor:      DrawTwoColorEditor();      break;
    case ColorMode::GradientCurve: DrawGradientCurveEditor(); break;
    }
}

void UpdateColor::DrawTwoColorEditor() {
    ImGui::ColorEdit4("開始色", &startColor_.x);
    ImGui::ColorEdit4("終了色", &endColor_.x);
    ImGui::TextDisabled("(開始色 → 終了色 を寿命に沿ってLinear補間)");
}

void UpdateColor::RebuildDelegate() {
    delegate_ = std::make_unique<CurveDelegate>();
    // ABGR 順（ImGui の色フォーマット: 0xAABBGGRR）
    delegate_->AddChannel(&curveR_.GetChannel(), 0xFF4444FF, "R");
    delegate_->AddChannel(&curveG_.GetChannel(), 0xFF44FF44, "G");
    delegate_->AddChannel(&curveB_.GetChannel(), 0xFFFF4444, "B");
    delegate_->AddChannel(&curveA_.GetChannel(), 0xFFAAAAAA, "A");
    delegate_->SetViewRange(ImVec2(0.0f, -0.1f), ImVec2(1.0f, 1.1f));
    delegateDirty_ = false;
}

void UpdateColor::DrawGradientCurveEditor() {
    if (delegateDirty_) RebuildDelegate();

    // ── 可視フラグ切り替え ──────────────────────────────────
    static const char* kChLabels[] = { "R", "G", "B", "A" };
    static const ImVec4 kChColors[] = {
        { 0.5f, 0.3f, 0.3f, 1.0f },
        { 0.3f, 0.5f, 0.3f, 1.0f },
        { 0.3f, 0.3f, 0.7f, 1.0f },
        { 0.6f, 0.6f, 0.6f, 1.0f },
    };
    for (int i = 0; i < 4; ++i) {
        bool vis = delegate_->GetVisible(i);
        ImGui::PushStyleColor(ImGuiCol_Text, kChColors[i]);
        if (ImGui::Checkbox(kChLabels[i], &vis))
            delegate_->SetVisible(i, vis);
        ImGui::PopStyleColor();
        if (i < 3) ImGui::SameLine();
    }

    // ── カーブエディタ ─────────────────────────────────────
    float w = ImGui::GetContentRegionAvail().x;
    ImCurveEdit::Edit(*delegate_, ImVec2(w, 180.0f),
        ImGui::GetID("##colorCurveEdit"));

    ImGui::Separator();

    // ── チャンネルごとの詳細設定 ───────────────────────────
    if (ImGui::TreeNode("チャンネル詳細")) {
        static const char* kInterpNames[] = { "Step", "Linear", "CatmullRom", "Bezier" };

        CurveProperty* channels[4] = { &curveR_, &curveG_, &curveB_, &curveA_ };
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Text, kChColors[i]);
            ImGui::Text("%s", kChLabels[i]);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            // チャンネルのモード（Constant / Random / Curve）
            static const char* kPropModes[] = { "固定", "ランダム", "カーブ" };
            int pm = static_cast<int>(channels[i]->GetMode());
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##pm", &pm, kPropModes, 3)) {
                channels[i]->SetMode(static_cast<CurveProperty::Mode>(pm));
                delegateDirty_ = true;
            }
            ImGui::SameLine();

            // 補間モード（Curve モード時のみ意味がある）
            int im = static_cast<int>(channels[i]->GetChannel().GetDefaultMode());
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("補間##im", &im, kInterpNames, 4)) {
                auto newMode = static_cast<InterpolationMode>(im);
                channels[i]->GetChannel().SetDefaultMode(newMode);
                for (auto& k : channels[i]->GetChannel().GetKeys())
                    k.interpMode = newMode;
                delegateDirty_ = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%d keys)", channels[i]->GetChannel().GetKeyCount());

            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

#endif // USE_IMGUI