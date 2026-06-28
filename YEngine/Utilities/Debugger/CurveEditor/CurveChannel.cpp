#include "CurveChannel.h"
#include <algorithm>
#include <cmath>

//=============================================================================
// 構築
//=============================================================================

CurveChannel::CurveChannel()
    : name_("Channel"), viewMin_(0.0f), viewMax_(1.0f)
{
    Reset();
}

CurveChannel::CurveChannel(const std::string& name, float viewMin, float viewMax)
    : name_(name), viewMin_(viewMin), viewMax_(viewMax)
{
    Reset();
}

//=============================================================================
// キー操作
//=============================================================================

void CurveChannel::AddKey(float time, float value, InterpolationMode mode,
                           float inTangent, float outTangent)
{
    keys_.emplace_back(time, value, mode, inTangent, outTangent);
    SortByTime();
}

void CurveChannel::RemoveKey(int index)
{
    if (index < 0 || index >= (int)keys_.size()) return;
    keys_.erase(keys_.begin() + index);
}

void CurveChannel::MoveKey(int index, float newTime, float newValue)
{
    if (index < 0 || index >= (int)keys_.size()) return;
    keys_[index].time  = newTime;
    keys_[index].value = newValue;
    SortByTime();
}

void CurveChannel::SetKeyTangents(int index, float inT, float outT)
{
    if (index < 0 || index >= (int)keys_.size()) return;
    keys_[index].inTangent  = inT;
    keys_[index].outTangent = outT;
}

void CurveChannel::SetKeyMode(int index, InterpolationMode mode)
{
    if (index < 0 || index >= (int)keys_.size()) return;
    keys_[index].interpMode = mode;
}

void CurveChannel::SortByTime()
{
    std::sort(keys_.begin(), keys_.end());
}

void CurveChannel::Clear()
{
    keys_.clear();
}

void CurveChannel::Reset(float constValue)
{
    keys_.clear();
    keys_.emplace_back(0.0f, constValue, defaultMode_);
    keys_.emplace_back(1.0f, constValue, defaultMode_);
}

//=============================================================================
// 評価
//=============================================================================

float CurveChannel::Evaluate(float t) const
{
    if (keys_.empty()) return 0.0f;
    // keys の範囲にクランプ
    t = std::max(keys_.front().time, std::min(keys_.back().time, t));
    return EvaluateAt(t);
}

float CurveChannel::EvaluateUnclamped(float t) const
{
    if (keys_.empty())            return 0.0f;
    if (t <= keys_.front().time)  return keys_.front().value;
    if (t >= keys_.back().time)   return keys_.back().value;
    return EvaluateAt(t);
}

float CurveChannel::EvaluateAt(float t) const
{
    if (keys_.size() == 1) return keys_[0].value;

    for (int i = 0; i < (int)keys_.size() - 1; ++i)
    {
        if (t >= keys_[i].time && t <= keys_[i + 1].time)
        {
            float segLen = keys_[i + 1].time - keys_[i].time;
            float localT = (segLen > 1e-7f) ? (t - keys_[i].time) / segLen : 0.0f;
            return EvaluateSegment(i, localT);
        }
    }
    return keys_.back().value;
}

float CurveChannel::EvaluateSegment(int i, float localT) const
{
    const CurveKey& k0 = keys_[i];
    const CurveKey& k1 = keys_[i + 1];

    switch (k0.interpMode)
    {
    case InterpolationMode::Step:
        return k0.value;

    case InterpolationMode::Linear:
        return k0.value + (k1.value - k0.value) * localT;

    case InterpolationMode::CatmullRom:
    {
        // 端点はクランプ（phantom を重複させる）
        float p0 = (i > 0)                     ? keys_[i - 1].value : k0.value;
        float p3 = (i + 2 < (int)keys_.size()) ? keys_[i + 2].value : k1.value;
        return CatmullRomInterp(p0, k0.value, k1.value, p3, localT);
    }

    case InterpolationMode::Bezier:
    {
        // エルミート→ベジェ変換：cp = value ± tangent * segLen / 3
        float segLen = keys_[i + 1].time - keys_[i].time;
        float cp1    = k0.value + k0.outTangent * segLen / 3.0f;
        float cp2    = k1.value - k1.inTangent  * segLen / 3.0f;
        return CubicBezierInterp(k0.value, cp1, cp2, k1.value, localT);
    }
    }
    return k0.value;
}

//=============================================================================
// 補間アルゴリズム
//=============================================================================

float CurveChannel::CatmullRomInterp(float p0, float p1,
                                      float p2, float p3, float t) const
{
    // 標準 Catmull-Rom 式
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * (
          (2.0f * p1)
        + (-p0 + p2)                          * t
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

float CurveChannel::CubicBezierInterp(float v0, float cp1,
                                       float cp2, float v1, float t) const
{
    // De Casteljau 多項式展開
    const float mt  = 1.0f - t;
    const float mt2 = mt  * mt;
    const float mt3 = mt2 * mt;
    const float t2  = t   * t;
    const float t3  = t2  * t;
    return mt3 * v0
         + 3.0f * mt2 * t  * cp1
         + 3.0f * mt  * t2 * cp2
         +              t3 * v1;
}

//=============================================================================
// JSON シリアライズ
//=============================================================================

nlohmann::json CurveChannel::SaveToJson() const
{
    nlohmann::json j;
    j["name"]        = name_;
    j["defaultMode"] = static_cast<int>(defaultMode_);
    j["viewMin"]     = viewMin_;
    j["viewMax"]     = viewMax_;
    j["keys"]        = nlohmann::json::array();

    for (const auto& k : keys_)
    {
        j["keys"].push_back({
            { "time",       k.time       },
            { "value",      k.value      },
            { "inTangent",  k.inTangent  },
            { "outTangent", k.outTangent },
            { "interpMode", static_cast<int>(k.interpMode) }
        });
    }
    return j;
}

void CurveChannel::LoadFromJson(const nlohmann::json& j)
{
    name_        = j.value("name",        name_);
    defaultMode_ = static_cast<InterpolationMode>(j.value("defaultMode", 1));
    viewMin_     = j.value("viewMin",     viewMin_);
    viewMax_     = j.value("viewMax",     viewMax_);

    keys_.clear();
    if (!j.contains("keys")) { Reset(); return; }

    for (const auto& kj : j["keys"])
    {
        CurveKey k;
        k.time       = kj.value("time",       0.0f);
        k.value      = kj.value("value",      0.0f);
        k.inTangent  = kj.value("inTangent",  0.0f);
        k.outTangent = kj.value("outTangent", 0.0f);
        k.interpMode = static_cast<InterpolationMode>(kj.value("interpMode", 1));
        keys_.push_back(k);
    }

    SortByTime();
    if (keys_.empty()) Reset();
}
