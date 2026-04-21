#pragma once

//=============================================================================
// DopeSheetEditor
// 汎用タイムライン（ドープシート）UI コンポーネント
//
// 【使い方】
//   1. std::vector<DopeSheet::DopeTrack> を用意してキーフレームを登録する
//   2. Draw() を呼ぶ
//   3. 戻り値が true なら tracks が変更されたのでデータに書き戻す
//=============================================================================

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <vector>
#include <functional>
#include <string>
#include "DopeTrack.h"

namespace DopeSheet
{

    //=============================================================================
    // Editor
    //=============================================================================
    class DopeSheetEditor
    {
    public:
        DopeSheetEditor() = default;

        //=========================================================================
        // メイン描画
        //=========================================================================
        bool Draw(
            const char* id,
            std::vector<DopeTrack>& tracks,
            int                      totalFrames,
            int                      fps = 60,
            float                    height = 0.0f
        );

        //=========================================================================
        // アクセッサ
        //=========================================================================
        int   GetSeekFrame() const { return seekFrame_; }
        void  SetSeekFrame(int f) { seekFrame_ = f; }

        float GetZoom() const { return zoomX_; }
        void  SetZoom(float z) { zoomX_ = z; }

        void  ResetView() { zoomX_ = 12.0f; scrollX_ = 0.0f; seekFrame_ = 0; }

        //=========================================================================
        // コールバック
        //=========================================================================
        using AddKeyCallback = std::function<void(int trackIdx, int frame, float value)>;
        using DeleteKeyCallback = std::function<void(int trackIdx, int keyIdx)>;
        using MoveKeyCallback = std::function<void(int trackIdx, int keyIdx, int newFrame)>;
        using SeekCallback = std::function<void(int frame)>;
        using SelectKeyCallback = std::function<void(int trackIdx, int keyIdx, bool selected)>;
        // fromFrame: ドラッグ前のフレーム番号, delta: 移動フレーム数（正負あり）
        using SummaryKeyMovedCallback = std::function<void(int fromFrame, int delta)>;

        void SetAddKeyCallback(AddKeyCallback    cb) { onAddKey_ = cb; }
        void SetDeleteKeyCallback(DeleteKeyCallback cb) { onDeleteKey_ = cb; }
        void SetMoveKeyCallback(MoveKeyCallback   cb) { onMoveKey_ = cb; }
        void SetSeekCallback(SeekCallback      cb) { onSeek_ = cb; }
        void SetSelectKeyCallback(SelectKeyCallback cb) { onSelectKey_ = cb; }
        void SetSummaryKeyMovedCallback(SummaryKeyMovedCallback cb) { onSummaryKeyMoved_ = cb; }

#ifdef USE_IMGUI
        using RulerOverlayCallback = std::function<void(ImDrawList*, ImVec2, float, int)>;
        void SetRulerOverlayCallback(RulerOverlayCallback cb) { onRulerOverlay_ = cb; }
#endif

        //=========================================================================
        // レイアウト定数
        //=========================================================================
        static constexpr float kRowH = 22.0f;
        static constexpr float kLabelW = 150.0f;
        static constexpr float kRulerH = 20.0f;
        static constexpr float kHeaderH = 20.0f;

    private:
#ifdef USE_IMGUI
        void DrawBackground(ImDrawList* dl, ImVec2 origin, float timelineW, const std::vector<DopeTrack>& tracks);
        void DrawRuler(ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames, int fps);
        void DrawAllTracks(ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames, std::vector<DopeTrack>& tracks, bool& anyChanged);
        void DrawSeekBar(ImDrawList* dl, ImVec2 origin, float timelineH, int totalFrames);
        void DrawAddKeyPopup(std::vector<DopeTrack>& tracks);

        bool DrawTrackRow(ImDrawList* dl, ImVec2 rowMin, float cellW,
            int totalFrames, DopeTrack& track, int trackIdx);

        bool DrawKey(ImDrawList* dl, ImVec2 center, float radius,
            ImU32 fillCol, ImU32 outlineCol,
            DopeKey& key, int trackIdx, int keyIdx,
            float cellW, int totalFrames);

        void DrawKeyBar(ImDrawList* dl, ImVec2 rowMin, float cellW,
            DopeKey& key, int trackIdx, int keyIdx,
            ImU32 fillCol, int totalFrames);

        ImU32 ResolveKeyColor(const DopeTrack& track, const DopeKey& key, bool hovered) const;
#endif

        float zoomX_ = 12.0f;
        float scrollX_ = 0.0f;
        int   seekFrame_ = 0;

        struct SelectionRequest { int trackIdx; int keyIdx; };
        std::vector<SelectionRequest> selectionRequests_;

        struct DragState
        {
            enum class Mode { None, Move, ResizeLeft, ResizeRight };

            int   trackIdx = -1;
            int   keyIdx = -1;
            bool  active = false;
            Mode  mode = Mode::None;
            int   startFrame = 0;       // ドラッグ開始時の key.frame
            int   startDuration = 0;    // ドラッグ開始時の key.duration
            float startMouseX = 0.0f;
        };
        DragState drag_;

        int   pendingTrackIdx_ = -1;
        int   pendingFrame_ = 0;
        float pendingValue_ = 0.0f;
        int   pendingDuration_ = 0;
        bool  showAddPopup_ = false;

        AddKeyCallback    onAddKey_;
        DeleteKeyCallback onDeleteKey_;
        MoveKeyCallback   onMoveKey_;
        SeekCallback      onSeek_;
        SelectKeyCallback onSelectKey_;
        SummaryKeyMovedCallback onSummaryKeyMoved_;

#ifdef USE_IMGUI
        RulerOverlayCallback onRulerOverlay_;
#endif
    };

} // namespace DopeSheet