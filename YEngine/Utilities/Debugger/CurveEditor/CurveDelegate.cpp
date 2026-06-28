#include "CurveDelegate.h"

#ifdef USE_IMGUI

//=============================================================================
// チャンネル管理
//=============================================================================

void CurveDelegate::AddChannel(CurveChannel* ch, uint32_t color,
                                const std::string& label)
{
    ChannelDesc desc;
    desc.channel = ch;
    desc.color   = color;
    desc.label   = label.empty() ? ch->GetName() : label;
    channels_.push_back(std::move(desc));
    SyncFromChannel(channels_.size() - 1);
}

void CurveDelegate::RemoveChannel(size_t index)
{
    if (index < channels_.size())
        channels_.erase(channels_.begin() + index);
}

void CurveDelegate::ClearChannels()
{
    channels_.clear();
}

void CurveDelegate::SyncAll()
{
    for (size_t i = 0; i < channels_.size(); ++i)
        SyncFromChannel(i);
}

void CurveDelegate::SetVisible(size_t index, bool v)
{
    if (index < channels_.size()) channels_[index].visible = v;
}

bool CurveDelegate::GetVisible(size_t index) const
{
    if (index < channels_.size()) return channels_[index].visible;
    return false;
}

//=============================================================================
// ImCurveEdit::Delegate 実装
//=============================================================================

size_t CurveDelegate::GetCurveCount()
{
    return channels_.size();
}

bool CurveDelegate::IsVisible(size_t idx)
{
    if (idx >= channels_.size()) return false;
    return channels_[idx].visible;
}

size_t CurveDelegate::GetPointCount(size_t idx)
{
    if (idx >= channels_.size()) return 0;
    return channels_[idx].cache.size();
}

uint32_t CurveDelegate::GetCurveColor(size_t idx)
{
    if (idx >= channels_.size()) return 0xFFFFFFFF;
    return channels_[idx].color;
}

ImVec2* CurveDelegate::GetPoints(size_t idx)
{
    if (idx >= channels_.size() || channels_[idx].cache.empty())
        return nullptr;
    return channels_[idx].cache.data();
}

ImCurveEdit::CurveType CurveDelegate::GetCurveType(size_t idx) const
{
    if (idx >= channels_.size() || !channels_[idx].channel)
        return ImCurveEdit::CurveLinear;

    switch (channels_[idx].channel->GetDefaultMode()) {
    case InterpolationMode::Step:       return ImCurveEdit::CurveDiscrete;
    case InterpolationMode::CatmullRom: return ImCurveEdit::CurveSmooth;
    case InterpolationMode::Bezier:     return ImCurveEdit::CurveBezier;
    default:                            return ImCurveEdit::CurveLinear;
    }
}

int CurveDelegate::EditPoint(size_t curveIdx, int ptIdx, ImVec2 val)
{
    if (curveIdx >= channels_.size()) return ptIdx;
    auto& cache = channels_[curveIdx].cache;
    if (ptIdx < 0 || ptIdx >= (int)cache.size()) return ptIdx;

    cache[ptIdx] = val;
    SyncToChannel(curveIdx);
    return ptIdx;
}

void CurveDelegate::AddPoint(size_t curveIdx, ImVec2 val)
{
    if (curveIdx >= channels_.size()) return;
    channels_[curveIdx].cache.push_back(val);
    SyncToChannel(curveIdx);
    SyncFromChannel(curveIdx); // re-sort
}

int CurveDelegate::DeletePoints(size_t curveIdx, bool* pts)
{
    if (curveIdx >= channels_.size()) return 0;
    auto& cache = channels_[curveIdx].cache;

    std::vector<ImVec2> keep;
    keep.reserve(cache.size());
    for (size_t i = 0; i < cache.size(); ++i)
        if (!pts[i]) keep.push_back(cache[i]);

    int removed = (int)(cache.size() - keep.size());
    cache = std::move(keep);
    SyncToChannel(curveIdx);
    return removed;
}

//=============================================================================
// 同期ヘルパー
//=============================================================================

void CurveDelegate::SyncFromChannel(size_t idx)
{
    if (idx >= channels_.size() || !channels_[idx].channel) return;
    auto& cache = channels_[idx].cache;
    cache.clear();
    for (const auto& k : channels_[idx].channel->GetKeys())
        cache.emplace_back(k.time, k.value);
}

void CurveDelegate::SyncToChannel(size_t idx)
{
    if (idx >= channels_.size() || !channels_[idx].channel) return;

    CurveChannel& ch   = *channels_[idx].channel;
    auto          mode = ch.GetDefaultMode();

    std::vector<CurveKey> newKeys;
    newKeys.reserve(channels_[idx].cache.size());
    for (const auto& p : channels_[idx].cache)
        newKeys.emplace_back(p.x, p.y, mode);

    ch.GetKeys() = std::move(newKeys);
    ch.SortByTime();
}

#endif // USE_IMGUI
