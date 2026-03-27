#include "CurveProperty.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include <ImCurveEdit.h>
#include <algorithm>  // std::swap

//=============================================================================
// anonymous namespace — CurveProperty 専用の単チャンネル Delegate
// ヘッダを汚さないよう .cpp 内に閉じ込める
//=============================================================================
namespace {

class SingleChannelDelegate : public ImCurveEdit::Delegate
{
public:
    explicit SingleChannelDelegate(CurveChannel& ch, uint32_t color = 0xFF00CCFF)
        : ch_(ch), color_(color)
    {
        SyncFromChannel();
        viewMin_ = ImVec2(0.0f, ch_.GetViewMin() - 0.1f);
        viewMax_ = ImVec2(1.0f, ch_.GetViewMax() + 0.1f);
    }

    // ImCurveEdit::Delegate の必須実装
    size_t   GetCurveCount()             override { return 1; }
    bool     IsVisible(size_t)           override { return true; }
    size_t   GetPointCount(size_t)       override { return cache_.size(); }
    uint32_t GetCurveColor(size_t)       override { return color_; }
    ImVec2*  GetPoints(size_t)           override { return cache_.data(); }
    ImVec2&  GetMin()                    override { return viewMin_; }
    ImVec2&  GetMax()                    override { return viewMax_; }

    ImCurveEdit::CurveType GetCurveType(size_t) const override
    {
        switch (ch_.GetDefaultMode()) {
        case InterpolationMode::Step:       return ImCurveEdit::CurveDiscrete;
        case InterpolationMode::CatmullRom: return ImCurveEdit::CurveSmooth;
        case InterpolationMode::Bezier:     return ImCurveEdit::CurveBezier;
        default:                            return ImCurveEdit::CurveLinear;
        }
    }

    int EditPoint(size_t, int ptIdx, ImVec2 val) override
    {
        if (ptIdx < 0 || ptIdx >= (int)cache_.size()) return ptIdx;
        cache_[ptIdx] = val;
        SyncToChannel();
        return ptIdx;
    }

    void AddPoint(size_t, ImVec2 val) override
    {
        cache_.push_back(val);
        SyncToChannel();
        SyncFromChannel(); // re-sort
    }

    int DeletePoints(size_t, bool* pts)
    {
        std::vector<ImVec2> keep;
        for (size_t i = 0; i < cache_.size(); ++i)
            if (!pts[i]) keep.push_back(cache_[i]);
        int removed = (int)(cache_.size() - keep.size());
        cache_ = keep;
        SyncToChannel();
        return removed;
    }

private:
    void SyncFromChannel()
    {
        cache_.clear();
        for (const auto& k : ch_.GetKeys())
            cache_.emplace_back(k.time, k.value);
    }

    void SyncToChannel()
    {
        auto mode = ch_.GetDefaultMode();
        std::vector<CurveKey> newKeys;
        newKeys.reserve(cache_.size());
        for (const auto& p : cache_)
            newKeys.emplace_back(p.x, p.y, mode);
        ch_.GetKeys() = newKeys;
        ch_.SortByTime();
    }

    CurveChannel&       ch_;
    uint32_t            color_;
    std::vector<ImVec2> cache_;
    ImVec2              viewMin_{ 0.0f, -0.1f };
    ImVec2              viewMax_{ 1.0f,  1.1f };
};

} // anonymous namespace
#endif // USE_IMGUI

//=============================================================================
// 構築
//=============================================================================

CurveProperty::CurveProperty()
    : mode_(Mode::Constant), valMin_(1.0f), valMax_(1.0f)
{
    channel_.Reset(1.0f);
}

CurveProperty::CurveProperty(float constValue, const std::string& channelName)
    : mode_(Mode::Constant), valMin_(constValue), valMax_(constValue)
{
    channel_.SetName(channelName);
    channel_.Reset(constValue);
}

//=============================================================================
// 評価
//=============================================================================

float CurveProperty::Evaluate(float t, float rand) const
{
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

nlohmann::json CurveProperty::SaveToJson() const
{
    nlohmann::json j;
    j["mode"]    = static_cast<int>(mode_);
    j["valMin"]  = valMin_;
    j["valMax"]  = valMax_;
    j["channel"] = channel_.SaveToJson();
    return j;
}

void CurveProperty::LoadFromJson(const nlohmann::json& j)
{
    mode_   = static_cast<Mode>(j.value("mode",   0));
    valMin_ = j.value("valMin", 1.0f);
    valMax_ = j.value("valMax", 1.0f);
    if (j.contains("channel"))
        channel_.LoadFromJson(j["channel"]);
}

//=============================================================================
// DrawEditor
//=============================================================================
#ifdef USE_IMGUI

void CurveProperty::DrawEditor(const char* label,
                                float editorHeight,
                                float dragSpeed,
                                float valueMin,
                                float valueMax)
{
    ImGui::PushID(label);

    // モードセレクタ
    static const char* kModeNames[] = { "固定値", "ランダム", "カーブ" };
    int modeIdx = static_cast<int>(mode_);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("##Mode", &modeIdx, kModeNames, 3))
        mode_ = static_cast<Mode>(modeIdx);

    ImGui::SameLine();
    ImGui::TextUnformatted(label[0] == '#' ? channel_.GetName().c_str() : label);

    switch (mode_)
    {
    case Mode::Constant:
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##Val", &valMin_, dragSpeed, valueMin, valueMax, "%.3f");
        valMax_ = valMin_;
        break;

    case Mode::RandomBetween:
    {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##Min", &valMin_, dragSpeed, valueMin, valueMax, "Min %.3f");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##Max", &valMax_, dragSpeed, valueMin, valueMax, "Max %.3f");
        if (valMin_ > valMax_) std::swap(valMin_, valMax_);
        break;
    }

    case Mode::Curve:
    {
        // 補間モード変更
        static const char* kInterpNames[] = { "Step", "Linear", "CatmullRom", "Bezier" };
        int interpIdx = static_cast<int>(channel_.GetDefaultMode());
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("補間##IP", &interpIdx, kInterpNames, 4)) {
            auto newMode = static_cast<InterpolationMode>(interpIdx);
            channel_.SetDefaultMode(newMode);
            for (auto& k : channel_.GetKeys())
                k.interpMode = newMode;
        }

        // 単チャンネルデリゲート（即時モード：毎フレーム生成して問題なし）
        SingleChannelDelegate del(channel_);
        float w = ImGui::GetContentRegionAvail().x;
        ImCurveEdit::Edit(del, ImVec2(w, editorHeight),
                          ImGui::GetID("##singleCurve"));
        break;
    }
    }

    ImGui::PopID();
}

#endif // USE_IMGUI
