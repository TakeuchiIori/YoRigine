#include "CurveProperty.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include <ImCurveEdit.h>
#include <algorithm>
#include <cstdio>

//=============================================================================
// anonymous namespace — CurveProperty 専用ヘルパー
//=============================================================================
namespace {

    //-----------------------------------------------------------------------------
    // SingleChannelDelegate
    // EditorConfig を受け取り viewMin/viewMax を config 準拠で設定する
    //-----------------------------------------------------------------------------
    class SingleChannelDelegate : public ImCurveEdit::Delegate
    {
    public:
        SingleChannelDelegate(CurveChannel& ch, const CurveProperty::EditorConfig& cfg)
            : ch_(ch), color_(cfg.curveColor) {
            SyncFromChannel();
            // config の値域に小さな余白を付けて表示範囲を設定
            float pad = (cfg.valueMax - cfg.valueMin) * 0.06f + 0.001f;
            viewMin_ = ImVec2(0.0f, cfg.valueMin - pad);
            viewMax_ = ImVec2(1.0f, cfg.valueMax + pad);
        }

        size_t   GetCurveCount()       override { return 1; }
        bool     IsVisible(size_t)     override { return true; }
        size_t   GetPointCount(size_t) override { return cache_.size(); }
        uint32_t GetCurveColor(size_t) override { return color_; }
        ImVec2* GetPoints(size_t)     override { return cache_.empty() ? nullptr : cache_.data(); }
        ImVec2& GetMin()              override { return viewMin_; }
        ImVec2& GetMax()              override { return viewMax_; }

        ImCurveEdit::CurveType GetCurveType(size_t) const override {
            switch (ch_.GetDefaultMode()) {
            case InterpolationMode::Step:       return ImCurveEdit::CurveDiscrete;
            case InterpolationMode::CatmullRom: return ImCurveEdit::CurveSmooth;
            case InterpolationMode::Bezier:     return ImCurveEdit::CurveBezier;
            default:                            return ImCurveEdit::CurveLinear;
            }
        }

        int EditPoint(size_t, int ptIdx, ImVec2 val) override {
            if (ptIdx >= 0 && ptIdx < (int)cache_.size()) {
                cache_[ptIdx] = val;
                SyncToChannel();
            }
            return ptIdx;
        }

        void AddPoint(size_t, ImVec2 val) override {
            cache_.push_back(val);
            SyncToChannel();
            SyncFromChannel(); // ソート反映
        }

        int DeletePoints(size_t, bool* pts) {
            std::vector<ImVec2> keep;
            keep.reserve(cache_.size());
            for (size_t i = 0; i < cache_.size(); ++i)
                if (!pts[i]) keep.push_back(cache_[i]);
            int removed = (int)(cache_.size() - keep.size());
            cache_ = std::move(keep);
            SyncToChannel();
            return removed;
        }

    private:
        void SyncFromChannel() {
            cache_.clear();
            for (const auto& k : ch_.GetKeys())
                cache_.emplace_back(k.time, k.value);
        }

        void SyncToChannel() {
            auto mode = ch_.GetDefaultMode();
            std::vector<CurveKey> newKeys;
            newKeys.reserve(cache_.size());
            for (const auto& p : cache_)
                newKeys.emplace_back(p.x, p.y, mode);
            ch_.GetKeys() = std::move(newKeys);
            ch_.SortByTime();
        }

        CurveChannel& ch_;
        uint32_t            color_;
        std::vector<ImVec2> cache_;
        ImVec2              viewMin_{ 0.0f, -0.1f };
        ImVec2              viewMax_{ 1.0f,  1.1f };
    };

    //-----------------------------------------------------------------------------
    // Y 軸目盛ラベルをカーブエディタの上にオーバーレイ描画
    // rMin/rMax : ImGui::GetItemRectMin/Max() で得た矩形（スクリーン座標）
    // vMin/vMax : ImCurveEdit の viewMin/Max の Y 成分（実値域）
    //-----------------------------------------------------------------------------
    static void DrawYLabels(ImVec2 rMin, ImVec2 rMax,
        float vMin, float vMax, int count) {
        const float vRange = vMax - vMin;
        if (vRange < 1e-6f || count <= 0) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int i = 0; i <= count; ++i) {
            float frac = static_cast<float>(i) / static_cast<float>(count);
            float val = vMax - vRange * frac;          // 上端 = 大きい値
            float sy = rMin.y + (rMax.y - rMin.y) * frac;

            // 短いティック
            dl->AddLine(
                ImVec2(rMin.x, sy),
                ImVec2(rMin.x + 6, sy),
                IM_COL32(200, 200, 200, 130)
            );
            // 薄いグリッドライン
            dl->AddLine(
                ImVec2(rMin.x + 6, sy),
                ImVec2(rMax.x, sy),
                IM_COL32(180, 180, 180, 35)
            );
            // 数値ラベル
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", val);
            dl->AddText(
                ImVec2(rMin.x + 8, sy - 6),
                IM_COL32(230, 230, 230, 210),
                buf
            );
        }
    }

    //-----------------------------------------------------------------------------
    // X 軸(時間)目盛ラベルをオーバーレイ描画
    //-----------------------------------------------------------------------------
    static void DrawXLabels(ImVec2 rMin, ImVec2 rMax, int count) {
        if (count <= 0) return;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int i = 0; i <= count; ++i) {
            float frac = static_cast<float>(i) / static_cast<float>(count);
            float sx = rMin.x + (rMax.x - rMin.x) * frac;

            // 短いティック（下端）
            dl->AddLine(
                ImVec2(sx, rMax.y - 6),
                ImVec2(sx, rMax.y),
                IM_COL32(200, 200, 200, 130)
            );
            // 薄いグリッドライン
            dl->AddLine(
                ImVec2(sx, rMin.y),
                ImVec2(sx, rMax.y - 6),
                IM_COL32(180, 180, 180, 35)
            );
            // 数値ラベル（下端の内側）
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%.2f", frac);
            float textX = (i == count) ? sx - 22 : sx + 2; // 右端は左寄せ
            dl->AddText(
                ImVec2(textX, rMax.y - 14),
                IM_COL32(230, 230, 230, 180),
                buf
            );
        }
    }

    //-----------------------------------------------------------------------------
    // 各キーポイントの値をカーブの点の近くにオーバーレイ表示
    //-----------------------------------------------------------------------------
    static void DrawKeyValueOverlay(ImVec2 rMin, ImVec2 rMax,
        const CurveChannel& ch,
        float vMin, float vMax) {
        const float vRange = vMax - vMin;
        if (vRange < 1e-6f) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (const auto& k : ch.GetKeys()) {
            // スクリーン座標へ変換 (t: 0..1 固定、値域は vMin..vMax)
            float fx = k.time;                              // 0〜1
            float fy = 1.0f - (k.value - vMin) / vRange;   // 上が大きい

            float sx = rMin.x + fx * (rMax.x - rMin.x);
            float sy = rMin.y + fy * (rMax.y - rMin.y);

            // ラベル（キーの丸より少し上右）
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.3f", k.value);

            // 黒縁→黄文字で視認性を確保
            dl->AddText(ImVec2(sx + 5, sy - 15), IM_COL32(0, 0, 0, 200), buf);
            dl->AddText(ImVec2(sx + 4, sy - 16), IM_COL32(255, 220, 60, 230), buf);
        }
    }

} // anonymous namespace
#endif // USE_IMGUI

//=============================================================================
// 構築
//=============================================================================

CurveProperty::CurveProperty()
    : mode_(Mode::Constant), valMin_(1.0f), valMax_(1.0f) {
    channel_.Reset(1.0f);
}

CurveProperty::CurveProperty(float constValue, const std::string& channelName)
    : mode_(Mode::Constant), valMin_(constValue), valMax_(constValue) {
    channel_.SetName(channelName);
    channel_.Reset(constValue);
}

//=============================================================================
// 評価
//=============================================================================

float CurveProperty::Evaluate(float t, float rand) const {
    switch (mode_) {
    case Mode::Constant:      return valMin_;
    case Mode::RandomBetween: return valMin_ + (valMax_ - valMin_) * rand;
    case Mode::Curve:         return channel_.Evaluate(t);
    }
    return valMin_;
}

//=============================================================================
// JSON
//=============================================================================

nlohmann::json CurveProperty::SaveToJson() const {
    nlohmann::json j;
    j["mode"] = static_cast<int>(mode_);
    j["valMin"] = valMin_;
    j["valMax"] = valMax_;
    j["channel"] = channel_.SaveToJson();
    return j;
}

void CurveProperty::LoadFromJson(const nlohmann::json& j) {
    mode_ = static_cast<Mode>(j.value("mode", 0));
    valMin_ = j.value("valMin", 1.0f);
    valMax_ = j.value("valMax", 1.0f);
    if (j.contains("channel"))
        channel_.LoadFromJson(j["channel"]);
}

//=============================================================================
// DrawEditor
//=============================================================================
#ifdef USE_IMGUI

void CurveProperty::DrawEditor(const char* label, const EditorConfig& cfg) {
    ImGui::PushID(label);

    //--- モードセレクタ ---
    static const char* kModeNames[] = { "固定値", "ランダム", "カーブ" };
    int modeIdx = static_cast<int>(mode_);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("##Mode", &modeIdx, kModeNames, 3))
        mode_ = static_cast<Mode>(modeIdx);

    ImGui::SameLine();
    ImGui::TextUnformatted(label[0] == '#' ? channel_.GetName().c_str() : label);

    switch (mode_) {
        //=========================================================================
    case Mode::Constant:
        //=========================================================================
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##Val", &valMin_, cfg.dragSpeed,
            cfg.valueMin, cfg.valueMax, "%.3f");
        valMax_ = valMin_;
        break;

        //=========================================================================
    case Mode::RandomBetween:
        //=========================================================================
    {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##Min", &valMin_, cfg.dragSpeed,
            cfg.valueMin, cfg.valueMax, "Min %.3f");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##Max", &valMax_, cfg.dragSpeed,
            cfg.valueMin, cfg.valueMax, "Max %.3f");
        if (valMin_ > valMax_) std::swap(valMin_, valMax_);
        break;
    }

    //=========================================================================
    case Mode::Curve:
        //=========================================================================
    {
        //--- 補間モード変更 ---
        static const char* kInterpNames[] = { "Step", "Linear", "CatmullRom", "Bezier" };
        int interpIdx = static_cast<int>(channel_.GetDefaultMode());
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("補間##IP", &interpIdx, kInterpNames, 4)) {
            auto newMode = static_cast<InterpolationMode>(interpIdx);
            channel_.SetDefaultMode(newMode);
            for (auto& k : channel_.GetKeys())
                k.interpMode = newMode;
        }

        //--- カーブエディタ本体 ---
        // 即時モードでデリゲートを生成（毎フレーム問題なし）
        SingleChannelDelegate del(channel_, cfg);
        float w = ImGui::GetContentRegionAvail().x;
        ImCurveEdit::Edit(del, ImVec2(w, cfg.editorHeight),
            ImGui::GetID("##singleCurve"));

        //--- オーバーレイ（エディタ widget の矩形を取得してから描く）---
        {
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            float  vMin = del.GetMin().y;
            float  vMax = del.GetMax().y;

            if (cfg.showYLabels)
                DrawYLabels(rMin, rMax, vMin, vMax, cfg.yLabelCount);

            if (cfg.showXLabels)
                DrawXLabels(rMin, rMax, cfg.xLabelCount);

            if (cfg.showKeyValues)
                DrawKeyValueOverlay(rMin, rMax, channel_, vMin, vMax);
        }

        //--- キー管理パネル ---
        if (cfg.showKeyList) {
            ImGui::Separator();

            // ── 追加コントロール ─────────────────────────────────────
            ImGui::PushID("##keyAdd");

            ImGui::TextUnformatted("追加:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(62.0f);
            ImGui::DragFloat("##addT", &addKeyTime_,
                0.005f, 0.0f, 1.0f, "T:%.3f");

            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f);
            ImGui::DragFloat("##addV", &addKeyValue_,
                cfg.dragSpeed,
                cfg.valueMin, cfg.valueMax, "V:%.3f");

            ImGui::SameLine();
            if (ImGui::SmallButton("+ 追加"))
                channel_.AddKey(addKeyTime_, addKeyValue_,
                    channel_.GetDefaultMode());

            ImGui::PopID();

            // ── キー一覧 ─────────────────────────────────────────────
            // ヘッダ行
            ImGui::Separator();
            ImGui::TextDisabled("  #   Time       Value");

            auto& keys = channel_.GetKeys();
            int   toDelete = -1;
            bool  needSort = false;

            for (int i = 0; i < (int)keys.size(); ++i) {
                ImGui::PushID(i);

                // インデックス
                char idxBuf[6];
                std::snprintf(idxBuf, sizeof(idxBuf), "[%d]", i);
                ImGui::TextDisabled("%s", idxBuf);
                ImGui::SameLine();

                // Time 編集
                ImGui::SetNextItemWidth(65.0f);
                if (ImGui::DragFloat("##t", &keys[i].time,
                    0.005f, 0.0f, 1.0f, "%.3f"))
                    needSort = true;
                ImGui::SameLine();

                // Value 編集
                ImGui::SetNextItemWidth(75.0f);
                ImGui::DragFloat("##v", &keys[i].value,
                    cfg.dragSpeed,
                    cfg.valueMin, cfg.valueMax, "%.3f");
                ImGui::SameLine();

                // 削除ボタン（minKeyCount 以下なら無効）
                const bool canDelete = (int)keys.size() > cfg.minKeyCount;
                if (!canDelete) ImGui::BeginDisabled();

                if (ImGui::SmallButton("削除"))
                    toDelete = i;

                if (!canDelete) {
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::TextDisabled("(最低%d点)", cfg.minKeyCount);
                }

                ImGui::PopID();
            }

            // ループ後にまとめて適用（ループ中のインデックス破壊を防ぐ）
            if (needSort)    channel_.SortByTime();
            if (toDelete >= 0) channel_.RemoveKey(toDelete);
        }
        break;
    }
    } // switch (mode_)

    ImGui::PopID();
}

#endif // USE_IMGUI