#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include "DopeSheetTypes.h"
#include "DopeKey.h"

namespace DopeSheet
{

    //=============================================================================
    // DopeTrack
    // タイムラインの 1 行分のデータ
    //=============================================================================
    struct DopeTrack
    {
        std::string label;
        TrackType   type = TrackType::Generic;
        Color       color = Color::White();
        std::vector<Color>   subColors;   // subType ごとの色（空なら color を使用）
        std::vector<DopeKey> keys;

        bool readOnly = false;
        bool visible = true;
        bool isGroupHeader = false;
        bool isSummary = false;   // 全KFを集約して表示する概要トラック
        bool groupExpanded = true;
        int  groupDepth = 0;

        //=========================================================================
        // ファクトリ
        //=========================================================================

        static DopeTrack Make(TrackType type, const std::string& customLabel = "")
        {
            DopeTrack t;
            t.type = type;
            t.color = GetDefaultTrackColor(type);
            t.label = customLabel.empty() ? GetDefaultTrackLabel(type) : customLabel;
            return t;
        }

        static DopeTrack MakeGroup(const std::string& label, int depth = 0)
        {
            DopeTrack t;
            t.label = label;
            t.isGroupHeader = true;
            t.groupDepth = depth;
            return t;
        }

        //=========================================================================
        // キー操作
        //=========================================================================

        void AddKey(int frame, float value = 0.0f, int subType = 0, int duration = 0)
        {
            keys.emplace_back(frame, value, subType, duration);
            SortKeys();
        }

        void AddKey(const DopeKey& key)
        {
            keys.push_back(key);
            SortKeys();
        }

        void RemoveKey(int index)
        {
            if (index >= 0 && index < static_cast<int>(keys.size()))
                keys.erase(keys.begin() + index);
        }

        void SortKeys()
        {
            std::sort(keys.begin(), keys.end(),
                [](const DopeKey& a, const DopeKey& b) { return a.frame < b.frame; });
        }

        void ClearKeys() { keys.clear(); }

        //=========================================================================
        // 検索
        //=========================================================================

        // 指定フレームと完全一致するキーのインデックス（なければ -1）
        int FindKeyAt(int frame) const
        {
            for (int i = 0; i < static_cast<int>(keys.size()); ++i)
                if (keys[i].frame == frame) return i;
            return -1;
        }

        // 指定フレームを区間内に含むキーのインデックス（なければ -1）
        int FindKeyContaining(int frame) const
        {
            for (int i = 0; i < static_cast<int>(keys.size()); ++i)
                if (keys[i].ContainsFrame(frame)) return i;
            return -1;
        }

        void ClearSelection()
        {
            for (auto& k : keys) k.selected = false;
        }
    };

} // namespace DopeSheet