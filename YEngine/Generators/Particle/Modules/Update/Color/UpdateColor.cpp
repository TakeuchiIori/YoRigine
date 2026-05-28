#include "UpdateColor.h"
#include "Loaders/Json/ConversionJson.h"
#ifdef USE_IMGUI
#include "imgui.h"
#include <ImCurveEdit.h>
#include <cstdio>
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
    const float t = attrs[index].GetNormalizedAge();
    const float rand = 0.5f;

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
	json["startColor"] = startColor_;
	json["endColor"] = endColor_;
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
    if (json.contains("startColor"))startColor_ = json["startColor"];
    if (json.contains("endColor")) endColor_ = json["endColor"];

    if (json.contains("curveR")) curveR_.LoadFromJson(json["curveR"]);
    if (json.contains("curveG")) curveG_.LoadFromJson(json["curveG"]);
    if (json.contains("curveB")) curveB_.LoadFromJson(json["curveB"]);
    if (json.contains("curveA")) curveA_.LoadFromJson(json["curveA"]);
}

//=============================================================================
// DrawEditor
//=============================================================================

void UpdateColor::DrawEditor() {
#ifdef USE_IMGUI
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
    delegate_->AddChannel(&curveR_.GetChannel(), 0xFF4444FF, "R");
    delegate_->AddChannel(&curveG_.GetChannel(), 0xFF44FF44, "G");
    delegate_->AddChannel(&curveB_.GetChannel(), 0xFFFF4444, "B");
    delegate_->AddChannel(&curveA_.GetChannel(), 0xFFAAAAAA, "A");
    delegate_->SetViewRange(ImVec2(0.0f, -0.06f), ImVec2(1.0f, 1.06f));
    delegateDirty_ = false;
}

void UpdateColor::DrawGradientCurveEditor() {
    if (delegateDirty_) RebuildDelegate();

    // ── 可視フラグ切り替え ────────────────────────────────────────────────
    static const char* kChLabels[] = { "R", "G", "B", "A" };
    static const ImVec4  kChColors[] = {
        { 0.9f, 0.4f, 0.4f, 1.0f },
        { 0.4f, 0.9f, 0.4f, 1.0f },
        { 0.4f, 0.5f, 1.0f, 1.0f },
        { 0.7f, 0.7f, 0.7f, 1.0f },
    };
    // キー値オーバーレイ用 IM_COL32（チャンネルごとに色を分ける）
    static const ImU32 kChOverlayColors[] = {
        IM_COL32(255, 120, 120, 230),
        IM_COL32(120, 255, 120, 230),
        IM_COL32(120, 160, 255, 230),
        IM_COL32(210, 210, 210, 230),
    };

    for (int i = 0; i < 4; ++i) {
        bool vis = delegate_->GetVisible(i);
        ImGui::PushStyleColor(ImGuiCol_Text, kChColors[i]);
        if (ImGui::Checkbox(kChLabels[i], &vis))
            delegate_->SetVisible(i, vis);
        ImGui::PopStyleColor();
        if (i < 3) ImGui::SameLine();
    }

    // ── 合成カーブエディタ（4チャンネル同時表示） ─────────────────────────
    float w = ImGui::GetContentRegionAvail().x;
    ImCurveEdit::Edit(*delegate_, ImVec2(w, 180.0f),
        ImGui::GetID("##colorCurveEdit"));

    // ── オーバーレイ（Y軸目盛 + チャンネルごとのキー値） ───────────────────
    {
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        float  vMin = delegate_->GetViewMinValue().y;
        float  vMax = delegate_->GetViewMaxValue().y;
        float  vRng = vMax - vMin;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Y 軸目盛（5分割）
        constexpr int kYDiv = 5;
        for (int i = 0; i <= kYDiv; ++i) {
            float frac = static_cast<float>(i) / kYDiv;
            float val = vMax - vRng * frac;
            float sy = rMin.y + (rMax.y - rMin.y) * frac;

            dl->AddLine(ImVec2(rMin.x, sy),
                ImVec2(rMin.x + 6, sy),
                IM_COL32(200, 200, 200, 130));
            dl->AddLine(ImVec2(rMin.x + 6, sy),
                ImVec2(rMax.x, sy),
                IM_COL32(180, 180, 180, 30));

            char buf[12];
            std::snprintf(buf, sizeof(buf), "%.2f", val);
            dl->AddText(ImVec2(rMin.x + 8, sy - 6),
                IM_COL32(230, 230, 230, 200), buf);
        }

        // 各チャンネルのキー値オーバーレイ（表示中チャンネルのみ）
        CurveProperty* channels[4] = { &curveR_, &curveG_, &curveB_, &curveA_ };
        for (int ci = 0; ci < 4; ++ci) {
            if (!delegate_->GetVisible(ci)) continue;
            for (const auto& k : channels[ci]->GetChannel().GetKeys()) {
                float fx = k.time;
                float fy = (vRng > 1e-6f) ? 1.0f - (k.value - vMin) / vRng : 0.0f;
                float sx = rMin.x + fx * (rMax.x - rMin.x);
                float sy = rMin.y + fy * (rMax.y - rMin.y);

                char buf[12];
                std::snprintf(buf, sizeof(buf), "%.2f", k.value);
                // 黒縁→チャンネル色
                dl->AddText(ImVec2(sx + 5, sy - 15), IM_COL32(0, 0, 0, 180), buf);
                dl->AddText(ImVec2(sx + 4, sy - 16), kChOverlayColors[ci], buf);
            }
        }
    }

    ImGui::Separator();

    // ── チャンネルごとの詳細エディタ ─────────────────────────────────────
    // CurveProperty::DrawEditor に委譲することで
    // モード選択・補間変更・キー追加/削除を自動的に取得する
    static const uint32_t kChCurveColors[] = {
        0xFF4444FF, 0xFF44FF44, 0xFFFF4444, 0xFFAAAAAA
    };

    if (ImGui::TreeNode("チャンネル詳細")) {
        CurveProperty* channels[4] = { &curveR_, &curveG_, &curveB_, &curveA_ };

        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            const ImVec4& c = kChColors[i];
            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(c.x, c.y, c.z, c.w * 0.3f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                ImVec4(c.x, c.y, c.z, c.w * 0.5f));

            // TreeNode 内に DrawEditor を展開
            if (ImGui::TreeNode(kChLabels[i])) {
                // 変化検知（delegateDirty_ のため）
                auto prevMode = channels[i]->GetMode();
                auto prevInterp = channels[i]->GetChannel().GetDefaultMode();
                int  prevKeys = channels[i]->GetChannel().GetKeyCount();

                CurveProperty::EditorConfig cfg;
                cfg.valueMin = 0.0f;
                cfg.valueMax = 1.0f;
                cfg.editorHeight = 100.0f;
                cfg.dragSpeed = 0.005f;
                cfg.curveColor = kChCurveColors[i];
                cfg.showYLabels = true;
                cfg.yLabelCount = 4;
                cfg.showXLabels = false;
                cfg.showKeyValues = true;
                cfg.showKeyList = true;
                cfg.minKeyCount = 2;

                channels[i]->DrawEditor(kChLabels[i], cfg);

                // モード・補間・キー数のいずれかが変わったら合成ビューを再構築
                if (channels[i]->GetMode() != prevMode ||
                    channels[i]->GetChannel().GetDefaultMode() != prevInterp ||
                    channels[i]->GetChannel().GetKeyCount() != prevKeys) {
                    delegateDirty_ = true;
                }

                ImGui::TreePop();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
#endif // USE_IMGUI
}
