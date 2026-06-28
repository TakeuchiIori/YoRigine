#pragma once
#ifdef USE_IMGUI

#include <ImCurveEdit.h>
#include "CurveChannel.h"
#include <vector>
#include <string>
#include <cstdint>

/// <summary>
/// ImCurveEdit への汎用アダプタ。
/// 複数の CurveChannel を同一ビューポートで同時表示・編集できる。
///
/// ■ 典型的な使用例（UpdateColor の RGBA 4 チャンネル）
///   CurveDelegate del;
///   del.AddChannel(&rCh, 0xFF4444FF, "R");
///   del.AddChannel(&gCh, 0xFF44FF44, "G");
///   del.AddChannel(&bCh, 0xFFFF4444, "B");
///   del.AddChannel(&aCh, 0xFFAAAAAA, "A");
///   del.SetViewRange(ImVec2(0,-0.1f), ImVec2(1,1.1f));
///   ImCurveEdit::Edit(del, ImVec2(w, h), id);
///
/// ■ 注意
///   channel ポインタの生存期間を管理するのは呼び出し側の責任。
///   SetColorMode / RebuildDelegate 時に SyncAll() を呼ぶこと。
/// </summary>
class CurveDelegate : public ImCurveEdit::Delegate
{
public:
    CurveDelegate() = default;

    //--- チャンネル情報 ---
    struct ChannelDesc {
        CurveChannel*       channel = nullptr;
        uint32_t            color   = 0xFFFFFFFF;
        std::string         label;
        bool                visible = true;
        std::vector<ImVec2> cache;  // ImCurveEdit に渡すキャッシュ
    };

    //===== チャンネル管理 =====
    void   AddChannel(CurveChannel* ch, uint32_t color,
                      const std::string& label = "");
    void   RemoveChannel(size_t index);
    void   ClearChannels();
    size_t GetChannelCount() const { return channels_.size(); }

    /// 全チャンネルのキャッシュを再構築（外部からキーを変更した後に呼ぶ）
    void SyncAll();

    //===== 表示範囲 =====
    void   SetViewRange(ImVec2 mn, ImVec2 mx) { viewMin_ = mn; viewMax_ = mx; }
    ImVec2 GetViewMinValue() const { return viewMin_; }
    ImVec2 GetViewMaxValue() const { return viewMax_; }

    //===== 可視フラグ =====
    void SetVisible(size_t index, bool v);
    bool GetVisible(size_t index) const;

    //===== ImCurveEdit::Delegate =====
    size_t   GetCurveCount()                                 override;
    bool     IsVisible(size_t curveIndex)                    override;
    size_t   GetPointCount(size_t curveIndex)                override;
    uint32_t GetCurveColor(size_t curveIndex)                override;
    ImVec2*  GetPoints(size_t curveIndex)                    override;
    ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const override;
    int      EditPoint(size_t curveIndex, int ptIndex, ImVec2 value) override;
    void     AddPoint(size_t curveIndex, ImVec2 value)       override;
    int      DeletePoints(size_t curveIndex, bool* points);
    ImVec2&  GetMin()                                        override { return viewMin_; }
    ImVec2&  GetMax()                                        override { return viewMax_; }

private:
    void SyncFromChannel(size_t index);
    void SyncToChannel(size_t index);

    std::vector<ChannelDesc> channels_;
    ImVec2 viewMin_{ 0.0f, -0.1f };
    ImVec2 viewMax_{ 1.0f,  1.1f };
};

#endif // USE_IMGUI
