#include "DopeSheetEditor.h"
#include <algorithm>
#include <cstdio>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

namespace DopeSheet
{

    //=============================================================================
    // メイン描画
    //=============================================================================
    bool DopeSheetEditor::Draw(
        const char* id,
        std::vector<DopeTrack>& tracks,
        int                     totalFrames,
        int                     fps,
        float                   height)
    {
#ifndef USE_IMGUI
        return false;
#else
        bool anyChanged = false;
        totalFrames = std::max(1, totalFrames);

        // 表示行数カウント
        int visibleRows = 0;
        {
            bool collapsed = false;
            for (const auto& t : tracks)
            {
                if (!t.visible) continue;
                if (t.isGroupHeader) { collapsed = !t.groupExpanded; visibleRows++; continue; }
                if (!collapsed) visibleRows++;
            }
        }

        const float timelineH = (height > 0.0f)
            ? height
            : kRulerH + kRowH * visibleRows + 4.0f;

        // ツールバー
        ImGui::PushID(id);
        ImGui::Text("ズーム: %.0fpx/f", zoomX_);
        ImGui::SameLine();
        ImGui::Text("| %d f  (%.2f s)", seekFrame_, seekFrame_ / (float)fps);
        ImGui::SameLine();
        if (ImGui::SmallButton("リセット")) ResetView();
        ImGui::SameLine();
        ImGui::TextDisabled("(Ctrl+ホイール で拡縮)");

        // スクロール子ウィンドウ
        const std::string childId = std::string("##DopeScroll_") + id;
        ImGui::BeginChild(
            childId.c_str(),
            { 0, timelineH + 4 },
            false,
            ImGuiWindowFlags_HorizontalScrollbar);

        const float windowW = ImGui::GetContentRegionAvail().x;

        // Ctrl + マウスホイール でズーム
        // scrollX_ を自前管理することで SetScrollX の1フレーム遅延を回避する
        if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl)
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                // カーソル下のフレームをズーム前に記録
                float mouseLocalX = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
                float frameAtMouse = (mouseLocalX + scrollX_ - kLabelW) / zoomX_;

                // ズーム適用
                zoomX_ = std::clamp(zoomX_ + wheel * 1.5f, 2.0f, 60.0f);

                // ズーム後も同じフレームがカーソル下に来るようスクロール補正
                scrollX_ = std::max(0.0f, frameAtMouse * zoomX_ - mouseLocalX + kLabelW);
            }
        }

        // 通常の横スクロール（Ctrl なし）は ImGui のスクロールバーと同期
        if (!ImGui::GetIO().KeyCtrl)
            scrollX_ = ImGui::GetScrollX();
        else
            ImGui::SetScrollX(scrollX_);

        // ズーム・スクロール確定後に timelineW を計算
        const float timelineW = kLabelW + totalFrames * zoomX_ + 20.0f;
        scrollX_ = std::min(scrollX_, std::max(0.0f, timelineW - windowW));

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // origin にスクロールを反映した描画基点を作る
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 origin = { winPos.x - scrollX_, winPos.y };

        ImGui::Dummy({ timelineW, timelineH });

        DrawBackground(dl, origin, timelineW, tracks);
        DrawRuler(dl, origin, zoomX_, totalFrames, fps);
        DrawAllTracks(dl, origin, zoomX_, totalFrames, tracks, anyChanged);
        DrawSeekBar(dl, origin, timelineH, totalFrames);

        // ルーラークリックでシーク
        {
            ImVec2 rMin = { origin.x + kLabelW, origin.y };
            ImVec2 rMax = { origin.x + timelineW, origin.y + kRulerH };
            if (ImGui::IsMouseHoveringRect(rMin, rMax) && ImGui::IsMouseDown(0))
            {
                int f = std::clamp(
                    static_cast<int>((ImGui::GetMousePos().x - rMin.x) / zoomX_),
                    0, totalFrames);
                if (f != seekFrame_)
                {
                    seekFrame_ = f;
                    if (onSeek_) onSeek_(seekFrame_);
                }
            }
        }

        DrawAddKeyPopup(tracks);
        ImGui::EndChild();
        ImGui::PopID();

        return anyChanged;
#endif
    }

    //=============================================================================
    // 背景縞模様
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawBackground(
        ImDrawList* dl, ImVec2 origin, float timelineW,
        const std::vector<DopeTrack>& tracks)
    {
        bool collapsed = false;
        int  row = 0;

        for (const auto& t : tracks)
        {
            if (!t.visible) continue;
            float y = origin.y + kRulerH + row * kRowH;

            if (t.isGroupHeader)
            {
                collapsed = !t.groupExpanded;
                dl->AddRectFilled(
                    { origin.x, y },
                    { origin.x + timelineW, y + kHeaderH },
                    IM_COL32(35, 55, 75, 255));
                row++;
                continue;
            }
            if (collapsed) continue;

            ImU32 bg = (row % 2 == 0) ? IM_COL32(46, 46, 46, 255) : IM_COL32(38, 38, 38, 255);
            dl->AddRectFilled({ origin.x, y }, { origin.x + timelineW, y + kRowH }, bg);
            row++;
        }
    }
#endif

    //=============================================================================
    // ルーラー
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawRuler(
        ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames, int fps)
    {
        const float totalW = kLabelW + totalFrames * cellW + 20.0f;

        dl->AddRectFilled(
            { origin.x, origin.y },
            { origin.x + totalW, origin.y + kRulerH },
            IM_COL32(30, 30, 30, 255));

        const int step = std::max(1, static_cast<int>(30.0f / cellW));

        for (int f = 0; f <= totalFrames; ++f)
        {
            float x = origin.x + kLabelW + f * cellW;
            bool  isMajor = (f % fps == 0);
            bool  isMedium = (f % 5 == 0);

            ImU32 gridCol = isMajor ? IM_COL32(100, 100, 100, 200)
                : isMedium ? IM_COL32(70, 70, 70, 150)
                : IM_COL32(50, 50, 50, 100);
            dl->AddLine({ x, origin.y + kRulerH }, { x, origin.y + kRulerH + kRowH * 64 }, gridCol);

            float tickH = isMajor ? kRulerH * 0.6f : kRulerH * 0.35f;
            dl->AddLine(
                { x, origin.y + kRulerH - tickH },
                { x, origin.y + kRulerH },
                IM_COL32(180, 180, 180, 200));

            if (f % step == 0)
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), isMajor ? "%ds" : "%d", isMajor ? f / fps : f);
                dl->AddText({ x + 2, origin.y + 2 }, IM_COL32(180, 180, 180, 255), buf);
            }
        }

        dl->AddLine(
            { origin.x + kLabelW, origin.y },
            { origin.x + kLabelW, origin.y + kRulerH },
            IM_COL32(80, 80, 80, 255));

        if (onRulerOverlay_)
            onRulerOverlay_(dl, origin, cellW, totalFrames);
    }
#endif

    //=============================================================================
    // 全トラック描画
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawAllTracks(
        ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames,
        std::vector<DopeTrack>& tracks, bool& anyChanged)
    {
        bool collapsed = false;
        int  row = 0;

        for (int ti = 0; ti < static_cast<int>(tracks.size()); ++ti)
        {
            DopeTrack& track = tracks[ti];
            if (!track.visible) continue;

            float y = origin.y + kRulerH + row * kRowH;

            // グループヘッダー
            if (track.isGroupHeader)
            {
                collapsed = !track.groupExpanded;

                float  indentX = origin.x + 4 + track.groupDepth * 12.0f;
                dl->AddText(
                    { indentX, y + kHeaderH * 0.5f - 6.0f },
                    IM_COL32(200, 200, 200, 255),
                    track.groupExpanded ? "v" : ">");
                dl->AddText(
                    { indentX + 14, y + 4 },
                    IM_COL32(230, 230, 230, 255),
                    track.label.c_str());

                ImGui::SetCursorScreenPos({ origin.x, y });
                ImGui::InvisibleButton(
                    (std::string("##grp_") + std::to_string(ti)).c_str(),
                    { kLabelW + totalFrames * cellW, kHeaderH });
                if (ImGui::IsItemClicked())
                {
                    track.groupExpanded = !track.groupExpanded;
                    anyChanged = true;
                }
                row++;
                continue;
            }

            if (collapsed) continue;

            // ラベル列
            {
                dl->AddRectFilled(
                    { origin.x, y },
                    { origin.x + 3.0f, y + kRowH },
                    track.color.ToImU32());

                float indentX = origin.x + 6 + track.groupDepth * 12.0f;
                dl->AddText({ indentX,      y + 4 }, IM_COL32(130, 130, 130, 200), GetTrackIcon(track.type));
                dl->AddText({ indentX + 28, y + 4 }, IM_COL32(180, 180, 180, 255), track.label.c_str());
            }

            // キーフレーム行
            if (DrawTrackRow(dl, { origin.x + kLabelW, y }, cellW, totalFrames, track, ti))
                anyChanged = true;

            row++;
        }
    }
#endif

    //=============================================================================
    // トラック 1 行
    //=============================================================================
#ifdef USE_IMGUI
    bool DopeSheetEditor::DrawTrackRow(
        ImDrawList* dl, ImVec2 rowMin, float cellW,
        int totalFrames, DopeTrack& track, int trackIdx)
    {
        bool changed = false;
        const float halfH = kRowH * 0.5f;
        const float radius = std::min(halfH * 0.55f, cellW * 0.45f);

        // ドラッグ終了
        if (drag_.active && drag_.trackIdx == trackIdx && !ImGui::IsMouseDown(0))
        {
            track.SortKeys();
            if (onMoveKey_ && drag_.keyIdx < static_cast<int>(track.keys.size()))
                onMoveKey_(trackIdx, drag_.keyIdx, track.keys[drag_.keyIdx].frame);
            drag_.active = false;
            changed = true;
        }

        int removeIdx = -1;
        for (int ki = 0; ki < static_cast<int>(track.keys.size()); ++ki)
        {
            DopeKey& key = track.keys[ki];
            ImU32    fillCol = ResolveKeyColor(track, key, false);
            bool     keyChanged = false;

            if (key.shape == KeyShape::Bar || key.duration > 0)
            {
                DrawKeyBar(dl, rowMin, cellW, key, trackIdx, ki, fillCol, totalFrames);
            }
            else
            {
                keyChanged = DrawKey(
                    dl,
                    { rowMin.x + key.frame * cellW, rowMin.y + halfH },
                    radius,
                    fillCol, IM_COL32(255, 255, 255, 60),
                    key, trackIdx, ki, cellW, totalFrames);
            }

            if (keyChanged && !track.readOnly) changed = true;

            if (key.selected && ImGui::IsKeyPressed(ImGuiKey_Delete) && !track.readOnly)
                removeIdx = ki;
        }

        if (removeIdx >= 0)
        {
            if (onDeleteKey_) onDeleteKey_(trackIdx, removeIdx);
            else              track.RemoveKey(removeIdx);
            changed = true;
        }

        // 右クリック → 追加ポップアップ
        if (!track.readOnly)
        {
            ImVec2 trackMax = { rowMin.x + totalFrames * cellW, rowMin.y + kRowH };
            if (ImGui::IsMouseHoveringRect(rowMin, trackMax) && ImGui::IsMouseClicked(1))
            {
                pendingFrame_ = std::clamp(
                    static_cast<int>((ImGui::GetMousePos().x - rowMin.x) / cellW), 0, totalFrames);
                pendingValue_ = 0.0f;
                pendingDuration_ = 10;
                pendingTrackIdx_ = trackIdx;
                showAddPopup_ = true;
                ImGui::OpenPopup("##DopeAddKey");
            }
        }

        return changed;
    }
#endif

    //=============================================================================
    // キー（ひし形・丸・三角）
    //=============================================================================
#ifdef USE_IMGUI
    bool DopeSheetEditor::DrawKey(
        ImDrawList* dl, ImVec2 center, float radius,
        ImU32 fillCol, ImU32 outlineCol,
        DopeKey& key, int trackIdx, int keyIdx,
        float cellW, int totalFrames)
    {
        bool  changed = false;
        float cx = center.x, cy = center.y;

        if (key.selected) outlineCol = IM_COL32(255, 255, 255, 200);

        switch (key.shape)
        {
        case KeyShape::Circle:
            dl->AddCircleFilled({ cx, cy }, radius, fillCol);
            dl->AddCircle({ cx, cy }, radius, outlineCol, 0, 1.5f);
            break;

        case KeyShape::Triangle:
            dl->AddTriangleFilled(
                { cx,          cy - radius },
                { cx + radius, cy + radius * 0.6f },
                { cx - radius, cy + radius * 0.6f }, fillCol);
            dl->AddTriangle(
                { cx,          cy - radius },
                { cx + radius, cy + radius * 0.6f },
                { cx - radius, cy + radius * 0.6f }, outlineCol, 1.2f);
            break;

        default: // Diamond
            dl->AddQuadFilled(
                { cx, cy - radius }, { cx + radius, cy },
                { cx, cy + radius }, { cx - radius, cy }, fillCol);
            dl->AddQuad(
                { cx, cy - radius }, { cx + radius, cy },
                { cx, cy + radius }, { cx - radius, cy }, outlineCol, 1.2f);
            break;
        }

        ImVec2 hitMin = { cx - radius - 4, cy - radius - 4 };
        ImVec2 hitMax = { cx + radius + 4, cy + radius + 4 };
        bool   hovered = ImGui::IsMouseHoveringRect(hitMin, hitMax);

        if (hovered)
        {
            dl->AddQuad(
                { cx, cy - radius - 2 }, { cx + radius + 2, cy },
                { cx, cy + radius + 2 }, { cx - radius - 2, cy },
                IM_COL32(255, 255, 255, 120), 1.5f);

            ImGui::BeginTooltip();
            ImGui::Text("frame=%d  value=%.3f  sub=%d", key.frame, key.value, key.subType);
            if (!key.tag.empty()) ImGui::Text("tag: %s", key.tag.c_str());
            ImGui::EndTooltip();

            if (ImGui::IsMouseClicked(0) && !drag_.active)
            {
                drag_ = { trackIdx, keyIdx, true,DragState::Mode::Move, key.frame, 0,ImGui::GetMousePos().x };
                key.selected = true;
                if (onSelectKey_) onSelectKey_(trackIdx, keyIdx, true);
            }
        }

        if (drag_.active && drag_.trackIdx == trackIdx && drag_.keyIdx == keyIdx
            && ImGui::IsMouseDown(0))
        {
            int newFrame = std::clamp(
                drag_.startFrame + static_cast<int>((ImGui::GetMousePos().x - drag_.startMouseX) / cellW),
                0, totalFrames);
            if (newFrame != key.frame) { key.frame = newFrame; changed = true; }
        }

        return changed;
    }
#endif

    //=============================================================================
    // キー（区間バー）
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawKeyBar(
        ImDrawList* dl, ImVec2 rowMin, float cellW,
        DopeKey& key, int trackIdx, int keyIdx,
        ImU32 fillCol, int totalFrames)
    {
        const float pad = 2.0f;
        const float edgeW = 6.0f;  // 左右リサイズハンドル幅

        float x0 = rowMin.x + key.frame * cellW + pad;
        float x1 = rowMin.x + key.EndFrame() * cellW - pad;
        float y0 = rowMin.y + pad;
        float y1 = rowMin.y + kRowH - pad;
        if (x1 <= x0) x1 = x0 + 4.0f;

        ImU32 outCol = key.selected
            ? IM_COL32(255, 255, 255, 200)
            : IM_COL32(255, 255, 255, 80);

        // ── 描画（ラベル列にはみ出さないようクリップ）──
        ImVec2 winPos = ImGui::GetWindowPos();
        float  clipLeft = winPos.x + kLabelW;
        dl->PushClipRect(
            { clipLeft,                          rowMin.y },
            { winPos.x + ImGui::GetWindowWidth(), rowMin.y + kRowH },
            true);

        dl->AddRectFilled({ x0, y0 }, { x1, y1 }, fillCol, 3.0f);
        dl->AddRect({ x0, y0 }, { x1, y1 }, outCol, 3.0f, 0, 1.5f);

        if ((x1 - x0) > 20.0f)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%df", key.duration);
            dl->AddText({ x0 + 4, y0 + 3 }, IM_COL32(255, 255, 255, 200), buf);
        }

        // ── リサイズハンドル強調 ──
        ImVec2 leftMin = { x0,         y0 };
        ImVec2 leftMax = { x0 + edgeW, y1 };
        ImVec2 rightMin = { x1 - edgeW, y0 };
        ImVec2 rightMax = { x1,         y1 };

        bool hovLeft = ImGui::IsMouseHoveringRect(leftMin, leftMax);
        bool hovRight = ImGui::IsMouseHoveringRect(rightMin, rightMax);

        if (!drag_.active && hovLeft)
            dl->AddRectFilled(leftMin, leftMax, IM_COL32(255, 255, 255, 60), 2.0f);
        if (!drag_.active && hovRight)
            dl->AddRectFilled(rightMin, rightMax, IM_COL32(255, 255, 255, 60), 2.0f);

        dl->PopClipRect();

        // ── カーソル変更（クリップ外でOK）──
        if (!drag_.active)
        {
            if (hovLeft || hovRight)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            else if (ImGui::IsMouseHoveringRect({ x0, y0 }, { x1, y1 }))
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // ── ツールチップ ──
        bool hovBar = ImGui::IsMouseHoveringRect({ x0, y0 }, { x1, y1 });
        if (hovBar && !drag_.active)
        {
            ImGui::BeginTooltip();
            ImGui::Text("start=%d  end=%d  dur=%d  value=%.3f",
                key.frame, key.EndFrame(), key.duration, key.value);
            if (!key.tag.empty()) ImGui::Text("tag: %s", key.tag.c_str());
            ImGui::EndTooltip();
        }

        // ── ドラッグ開始 ──
        if (!drag_.active && ImGui::IsMouseClicked(0))
        {
            if (hovLeft)
            {
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::ResizeLeft,
                          key.frame, key.duration, ImGui::GetMousePos().x };
                key.selected = true;
            }
            else if (hovRight)
            {
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::ResizeRight,
                          key.frame, key.duration, ImGui::GetMousePos().x };
                key.selected = true;
            }
            else if (hovBar)
            {
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::Move,
                          key.frame, key.duration, ImGui::GetMousePos().x };
                key.selected = true;
                if (onSelectKey_) onSelectKey_(trackIdx, keyIdx, true);
            }
        }

        // ── ドラッグ中の更新 ──
        if (drag_.active && drag_.trackIdx == trackIdx && drag_.keyIdx == keyIdx
            && ImGui::IsMouseDown(0))
        {
            int delta = static_cast<int>(
                (ImGui::GetMousePos().x - drag_.startMouseX) / cellW);

            switch (drag_.mode)
            {
            case DragState::Mode::Move:
                key.frame = std::clamp(drag_.startFrame + delta, 0, totalFrames);
                break;

            case DragState::Mode::ResizeLeft:
            {
                int newFrame = std::clamp(drag_.startFrame + delta, 0,
                    drag_.startFrame + drag_.startDuration - 1);
                key.duration = drag_.startDuration - (newFrame - drag_.startFrame);
                key.frame = newFrame;
                break;
            }
            case DragState::Mode::ResizeRight:
                key.duration = std::clamp(
                    drag_.startDuration + delta, 1, totalFrames - drag_.startFrame);
                break;

            default: break;
            }
        }
    }
#endif

    //=============================================================================
    // シークバー
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawSeekBar(
        ImDrawList* dl, ImVec2 origin, float timelineH, int totalFrames)
    {
        if (seekFrame_ < 0 || seekFrame_ > totalFrames) return;

        float sx = origin.x + kLabelW + seekFrame_ * zoomX_;

        // ── 描画 ──
        dl->AddTriangleFilled(
            { sx - 5, origin.y }, { sx + 5, origin.y }, { sx, origin.y + 10 },
            IM_COL32(255, 220, 50, 230));
        dl->AddLine(
            { sx, origin.y + 10 }, { sx, origin.y + timelineH },
            IM_COL32(255, 220, 50, 180), 1.5f);

        // ── ドラッグ判定 ──
        // 三角形のヒット領域
        ImVec2 hitMin = { sx - 6, origin.y };
        ImVec2 hitMax = { sx + 6, origin.y + 12 };
        bool hovered = ImGui::IsMouseHoveringRect(hitMin, hitMax);

        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        // ドラッグ中（シークバー専用フラグで他のドラッグと区別）
        static bool seekDragging = false;

        if (hovered && ImGui::IsMouseClicked(0))
            seekDragging = true;

        if (seekDragging)
        {
            if (ImGui::IsMouseDown(0))
            {
                // マウスX座標からフレームを逆算
                float mouseX = ImGui::GetMousePos().x;
                float relX = mouseX - (origin.x + kLabelW);
                int   newFrame = std::clamp(
                    static_cast<int>(relX / zoomX_), 0, totalFrames);

                if (newFrame != seekFrame_)
                {
                    seekFrame_ = newFrame;
                    if (onSeek_) onSeek_(seekFrame_);
                }
            }
            else
            {
                seekDragging = false;
            }
        }
    }
#endif

    //=============================================================================
    // キー追加ポップアップ
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawAddKeyPopup(std::vector<DopeTrack>& tracks)
    {
        if (!ImGui::BeginPopup("##DopeAddKey")) return;

        const bool  valid = (pendingTrackIdx_ >= 0 &&
            pendingTrackIdx_ < static_cast<int>(tracks.size()));
        const char* label = valid ? tracks[pendingTrackIdx_].label.c_str() : "?";

        ImGui::Text("キー追加  [%s]  frame=%d", label, pendingFrame_);
        ImGui::Separator();
        ImGui::SetNextItemWidth(100); ImGui::InputFloat("値", &pendingValue_, 0.1f);
        ImGui::SetNextItemWidth(100); ImGui::InputInt("持続(f)", &pendingDuration_);
        pendingDuration_ = std::max(0, pendingDuration_);
        ImGui::Separator();

        if (ImGui::Button("追加") && valid)
        {
            if (onAddKey_)
            {
                onAddKey_(pendingTrackIdx_, pendingFrame_, pendingValue_);
            }
            else
            {
                auto& t = tracks[pendingTrackIdx_];
                if (!t.readOnly) t.AddKey(pendingFrame_, pendingValue_, 0, pendingDuration_);
            }
            pendingValue_ = 0.0f; pendingDuration_ = 0; showAddPopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) { showAddPopup_ = false; ImGui::CloseCurrentPopup(); }

        ImGui::EndPopup();
    }
#endif

    //=============================================================================
    // キー色の解決
    //=============================================================================
#ifdef USE_IMGUI
    ImU32 DopeSheetEditor::ResolveKeyColor(
        const DopeTrack& track, const DopeKey& key, bool hovered) const
    {
        Color col = (!track.subColors.empty() &&
            key.subType >= 0 &&
            key.subType < static_cast<int>(track.subColors.size()))
            ? track.subColors[key.subType]
            : track.color;

        if (hovered) col = col.Brightened(0.3f);
        return col.ToImU32();
    }
#endif

} // namespace DopeSheet