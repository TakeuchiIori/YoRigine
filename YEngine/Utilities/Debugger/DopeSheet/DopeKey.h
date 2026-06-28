#pragma once

#include <algorithm>
#include <string>
#include "DopeSheetTypes.h"

namespace DopeSheet
{

//=============================================================================
// DopeKey
// タイムライン上のキーフレーム 1 つ分のデータ
//=============================================================================
struct DopeKey
{
    int      frame    = 0;
    int      duration = 0;        // 0=単発、>0=区間バー表示
    float    value    = 0.0f;     // 汎用値（SE番号・強度など）
    int      subType  = 0;        // subColors での色分け用
    bool     selected = false;
    KeyShape shape    = KeyShape::Diamond;
    std::string tag;              // 任意のメモ

    DopeKey() = default;
    DopeKey(int f, float v = 0.0f, int sub = 0, int dur = 0)
        : frame(f), value(v), subType(sub), duration(dur) {
    }

    // 区間の終了フレーム
    int EndFrame() const { return frame + std::max(0, duration); }

    // 指定フレームがこのキーの区間内か
    bool ContainsFrame(int f) const { return f >= frame && f <= EndFrame(); }
};

} // namespace DopeSheet
