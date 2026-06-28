#include "DopeSheetEditor.h"
#include <algorithm>
#include <cstdio>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif
#include <Editor/Icon/EditorIcon.h>

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
        float                   height) {
#ifndef USE_IMGUI
        // ImGui を使わないビルドでは何も描画せず false を返すだけ
        (void) id; (void)tracks; (void)totalFrames; (void)fps; (void)height;
        return false;
#else
        bool anyChanged = false;
        totalFrames = std::max(1, totalFrames);

        // 表示行数カウント
        int visibleRows = 0;
        {
            bool collapsed = false;
            for (const auto& t : tracks) {
                if (!t.visible) continue;
                if (t.isGroupHeader) { collapsed = !t.groupExpanded; visibleRows++; continue; }
                if (!collapsed) visibleRows++;
            }
        }

        const float timelineH = (height > 0.0f)
            ? height
            : kRulerH + kRowH * visibleRows + 4.0f;

        ImGui::PushID(id);

        // ── ツールバー ──
        ImGui::Text("ズーム: %.0fpx/f", zoomX_);
        ImGui::SameLine();
        ImGui::Text("| %d f  (%.2f s)", seekFrame_, seekFrame_ / static_cast<float>(fps));
        ImGui::SameLine();
        if (ImGui::SmallButton("リセット")) ResetView();
        ImGui::SameLine();
        ImGui::TextDisabled("(Ctrl+ホイール で拡縮)");

        // =========================================================
        // 左ペイン：ラベル列（固定・スクロールなし）
        // =========================================================
        const std::string labelId = std::string("##DopeLabel_") + id;
        ImGui::BeginChild(
            labelId.c_str(),
            { kLabelW, timelineH + 4 },
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();

            // ルーラー部分の背景（ラベル側）
            dl->AddRectFilled(
                { winPos.x, winPos.y },
                { winPos.x + kLabelW, winPos.y + kRulerH },
                IM_COL32(30, 30, 30, 255));

            // 各トラックのラベルを描画
            bool collapsed = false;
            int  row = 0;
            for (int ti = 0; ti < static_cast<int>(tracks.size()); ++ti) {
                const DopeTrack& track = tracks[ti];
                if (!track.visible) continue;

                float y = winPos.y + kRulerH + row * kRowH;

                if (track.isGroupHeader) {
                    collapsed = !track.groupExpanded;
                    dl->AddRectFilled(
                        { winPos.x, y },
                        { winPos.x + kLabelW, y + kHeaderH },
                        IM_COL32(35, 55, 75, 255));

                    float indentX = winPos.x + 4 + track.groupDepth * 12.0f;
                    dl->AddText(
                        { indentX, y + kHeaderH * 0.5f - 6.0f },
                        IM_COL32(200, 200, 200, 255),
                        track.groupExpanded ? "v" : ">");
                    dl->AddText(
                        { indentX + 14, y + 4 },
                        IM_COL32(230, 230, 230, 255),
                        track.label.c_str());
                    row++;
                    continue;
                }

                // ── 概要トラック（特殊表示）──
                if (track.isSummary) {
                    dl->AddRectFilled(
                        { winPos.x, y },
                        { winPos.x + kLabelW, y + kRowH },
                        IM_COL32(52, 44, 16, 255));
                    // 左端のゴールドバー（通常より太め）
                    dl->AddRectFilled(
                        { winPos.x, y },
                        { winPos.x + 4.0f, y + kRowH },
                        IM_COL32(255, 210, 60, 255));
                    dl->PushClipRect({ winPos.x, y }, { winPos.x + kLabelW, y + kRowH }, true);
                    float indentX = winPos.x + 8.0f;
                    dl->AddText({ indentX,      y + 4 }, IM_COL32(255, 210, 60, 255), Icon::CheckCircle);
                    dl->AddText({ indentX + 18, y + 4 }, IM_COL32(255, 224, 100, 255), track.label.c_str());
                    dl->PopClipRect();
                    row++;
                    continue;
                }
                if (collapsed) continue;

                // 行背景
                ImU32 bg = (row % 2 == 0)
                    ? IM_COL32(46, 46, 46, 255)
                    : IM_COL32(38, 38, 38, 255);
                dl->AddRectFilled(
                    { winPos.x, y },
                    { winPos.x + kLabelW, y + kRowH }, bg);

                // 左端のカラーバー
                dl->AddRectFilled(
                    { winPos.x, y },
                    { winPos.x + 3.0f, y + kRowH },
                    track.color.ToImU32());

                // アイコン・ラベルテキスト（ラベル列内にクリップ）
                dl->PushClipRect(
                    { winPos.x, y },
                    { winPos.x + kLabelW, y + kRowH },
                    true);

                float indentX = winPos.x + 6 + track.groupDepth * 12.0f;
                dl->AddText({ indentX,      y + 4 },
                    IM_COL32(130, 130, 130, 200), GetTrackIcon(track.type));
                dl->AddText({ indentX + 28, y + 4 },
                    IM_COL32(180, 180, 180, 255), track.label.c_str());

                dl->PopClipRect();
                row++;
            }
        }

        ImGui::EndChild();
        ImGui::SameLine(0, 0); // 隙間なく横に並べる

        // =========================================================
        // 右ペイン：タイムライン列（横スクロールあり）
        // =========================================================
        const std::string timelineId = std::string("##DopeScroll_") + id;
        ImGui::BeginChild(
            timelineId.c_str(),
            { 0, timelineH + 4 },
            false,
            ImGuiWindowFlags_HorizontalScrollbar);

        const float windowW = ImGui::GetContentRegionAvail().x;
        const float timelineW = totalFrames * zoomX_ + 20.0f;

        // Ctrl + マウスホイール でズーム
        if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                float mouseLocalX = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
                float frameAtMouse = (mouseLocalX + scrollX_) / zoomX_;

                zoomX_ = std::clamp(zoomX_ + wheel * 1.5f, 2.0f, 60.0f);
                scrollX_ = std::max(0.0f, frameAtMouse * zoomX_ - mouseLocalX);
            }
        }

        // 通常スクロールは ImGui のスクロールバーと同期
        if (!ImGui::GetIO().KeyCtrl)
            scrollX_ = ImGui::GetScrollX();
        else
            ImGui::SetScrollX(scrollX_);

        scrollX_ = std::min(scrollX_, std::max(0.0f, timelineW - windowW));

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // origin はタイムライン列の左端（ラベル列を含まない）
        const ImVec2 winPos = ImGui::GetWindowPos();
        const ImVec2 origin = { winPos.x - scrollX_, winPos.y };

        ImGui::Dummy({ timelineW, timelineH });

        DrawBackground(dl, origin, timelineW, tracks);
        DrawRuler(dl, origin, zoomX_, totalFrames, fps);
        DrawAllTracks(dl, origin, zoomX_, totalFrames, tracks, anyChanged);
        DrawSeekBar(dl, origin, timelineH, totalFrames);

        // ルーラークリックでシーク
        {
            ImVec2 rMin = { origin.x, winPos.y };
            ImVec2 rMax = { origin.x + timelineW, winPos.y + kRulerH };
            if (ImGui::IsMouseHoveringRect(rMin, rMax) && ImGui::IsMouseDown(0)) {
                int f = std::clamp(
                    static_cast<int>((ImGui::GetMousePos().x - rMin.x) / zoomX_),
                    0, totalFrames);
                if (f != seekFrame_) {
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
    // 背景縞模様（タイムライン列用）
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawBackground(
        ImDrawList* dl, ImVec2 origin, float timelineW,
        const std::vector<DopeTrack>& tracks) {
        bool collapsed = false;
        int  row = 0;

        for (const auto& t : tracks) {
            if (!t.visible) continue;
            float y = origin.y + kRulerH + row * kRowH;

            if (t.isGroupHeader) {
                collapsed = !t.groupExpanded;
                dl->AddRectFilled(
                    { origin.x, y },
                    { origin.x + timelineW, y + kHeaderH },
                    IM_COL32(35, 55, 75, 255));
                row++;
                continue;
            }

            // 概要トラックはゴールド系背景
            if (t.isSummary) {
                dl->AddRectFilled(
                    { origin.x, y },
                    { origin.x + timelineW, y + kRowH },
                    IM_COL32(48, 40, 14, 255));
                row++;
                continue;
            }
            if (collapsed) continue;

            ImU32 bg = (row % 2 == 0)
                ? IM_COL32(46, 46, 46, 255)
                : IM_COL32(38, 38, 38, 255);
            dl->AddRectFilled(
                { origin.x, y },
                { origin.x + timelineW, y + kRowH }, bg);
            row++;
        }
    }
#endif

    //=============================================================================
    // ルーラー（タイムライン列用・kLabelW オフセットなし）
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawRuler(
        ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames, int fps) {
        const float totalW = totalFrames * cellW + 20.0f;

        dl->AddRectFilled(
            { origin.x, origin.y },
            { origin.x + totalW, origin.y + kRulerH },
            IM_COL32(30, 30, 30, 255));

        const int step = std::max(1, static_cast<int>(30.0f / cellW));

        for (int f = 0; f <= totalFrames; ++f) {
            float x = origin.x + f * cellW;  // kLabelW オフセット不要
            bool  isMajor = (f % fps == 0);
            bool  isMedium = (f % 5 == 0);

            ImU32 gridCol = isMajor ? IM_COL32(100, 100, 100, 200)
                : isMedium ? IM_COL32(70, 70, 70, 150)
                : IM_COL32(50, 50, 50, 100);
            dl->AddLine(
                { x, origin.y + kRulerH },
                { x, origin.y + kRulerH + kRowH * 64 },
                gridCol);

            float tickH = isMajor ? kRulerH * 0.6f : kRulerH * 0.35f;
            dl->AddLine(
                { x, origin.y + kRulerH - tickH },
                { x, origin.y + kRulerH },
                IM_COL32(180, 180, 180, 200));

            if (f % step == 0) {
                char buf[16];
                std::snprintf(buf, sizeof(buf),
                    isMajor ? "%ds" : "%d",
                    isMajor ? f / fps : f);
                dl->AddText({ x + 2, origin.y + 2 },
                    IM_COL32(180, 180, 180, 255), buf);
            }
        }

        if (onRulerOverlay_)
            onRulerOverlay_(dl, origin, cellW, totalFrames);
    }
#endif

    //=============================================================================
    // 全トラック描画（タイムライン列のみ・ラベルは左ペインで描画済み）
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawAllTracks(
        ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames,
        std::vector<DopeTrack>& tracks, bool& anyChanged) {
        bool collapsed = false;
        int  row = 0;

        for (int ti = 0; ti < static_cast<int>(tracks.size()); ++ti) {
            DopeTrack& track = tracks[ti];
            if (!track.visible) continue;

            float y = origin.y + kRulerH + row * kRowH;

            if (track.isGroupHeader) {
                collapsed = !track.groupExpanded;

                // グループヘッダーのクリック判定
                ImGui::SetCursorScreenPos({ origin.x, y });
                ImGui::InvisibleButton(
                    (std::string("##grp_") + std::to_string(ti)).c_str(),
                    { totalFrames * cellW, kHeaderH });
                if (ImGui::IsItemClicked()) {
                    track.groupExpanded = !track.groupExpanded;
                    anyChanged = true;
                }
                row++;
                continue;
            }
            if (collapsed) continue;

            // キーフレーム行（origin.x がタイムライン列の左端）
            if (DrawTrackRow(dl, { origin.x, y }, cellW, totalFrames, track, ti))
                anyChanged = true;

            row++;
        }

        // いずれかのキーがクリックされた → 全トラックの選択をクリアしてから
        // selectionRequests_ に積まれたキーだけ selected = true に戻す
        if (!selectionRequests_.empty()) {
            for (auto& t : tracks)
                for (auto& k : t.keys)
                    k.selected = false;

            for (const auto& req : selectionRequests_) {
                if (req.trackIdx >= 0 && req.trackIdx < static_cast<int>(tracks.size())) {
                    auto& dkeys = tracks[req.trackIdx].keys;
                    if (req.keyIdx >= 0 && req.keyIdx < static_cast<int>(dkeys.size()))
                        dkeys[req.keyIdx].selected = true;
                }
            }
            selectionRequests_.clear();
        }
    }
#endif

    //=============================================================================
    // トラック 1 行
    //=============================================================================
#ifdef USE_IMGUI
    bool DopeSheetEditor::DrawTrackRow(
        ImDrawList* dl, ImVec2 rowMin, float cellW,
        int totalFrames, DopeTrack& track, int trackIdx) {
        bool changed = false;
        const float halfH = kRowH * 0.5f;
        const float radius = std::min(halfH * 0.55f, cellW * 0.45f);

        // =====================================================================
        // 概要トラック専用処理
        // 全KF時刻を大きなダイヤモンドで表示し、ドラッグで全ボーン一括移動する
        // =====================================================================
        if (track.isSummary) {
            const float sumRadius = radius * 1.5f; // 通常より1.5倍大きく

            // ── ドラッグ終了 ──
            if (drag_.active && drag_.trackIdx == trackIdx && !ImGui::IsMouseDown(0)) {
                if (drag_.keyIdx < static_cast<int>(track.keys.size())) {
                    int endFrame = track.keys[drag_.keyIdx].frame;
                    int delta = endFrame - drag_.startFrame;
                    if (delta != 0 && onSummaryKeyMoved_) {
                        onSummaryKeyMoved_(drag_.startFrame, delta);
                    }
                    // サマリーキーはTimelinePanel側で再構築するので元の位置に戻す
                    track.keys[drag_.keyIdx].frame = drag_.startFrame;
                }
                drag_.active = false;
            }

            for (int ki = 0; ki < static_cast<int>(track.keys.size()); ++ki) {
                DopeKey& key = track.keys[ki];
                float    cx = rowMin.x + key.frame * cellW;
                float    cy = rowMin.y + halfH;

                bool isDraggingThis = (drag_.active && drag_.trackIdx == trackIdx && drag_.keyIdx == ki);

                // ドラッグ中はビジュアルフィードバックとして一時的に位置を更新
                if (isDraggingThis && ImGui::IsMouseDown(0)) {
                    int newFrame = std::clamp(
                        drag_.startFrame + static_cast<int>(
                            (ImGui::GetMousePos().x - drag_.startMouseX) / cellW),
                        0, totalFrames);
                    if (newFrame != key.frame) { key.frame = newFrame; changed = true; }
                    cx = rowMin.x + key.frame * cellW;
                }

                bool   hovered = false;
                ImVec2 hitMin = { cx - sumRadius - 4, cy - sumRadius - 4 };
                ImVec2 hitMax = { cx + sumRadius + 4, cy + sumRadius + 4 };
                hovered = ImGui::IsMouseHoveringRect(hitMin, hitMax);

                // 色: ホバー/選択で明るく
                ImU32 fillCol = hovered || key.selected
                    ? IM_COL32(255, 240, 100, 255)
                    : IM_COL32(255, 200, 50, 230);
                ImU32 outCol = hovered || key.selected
                    ? IM_COL32(255, 255, 200, 220)
                    : IM_COL32(200, 160, 30, 180);

                // 大きなダイヤモンド
                dl->AddQuadFilled(
                    { cx,              cy - sumRadius },
                    { cx + sumRadius,  cy },
                    { cx,              cy + sumRadius },
                    { cx - sumRadius,  cy }, fillCol);
                dl->AddQuad(
                    { cx,              cy - sumRadius },
                    { cx + sumRadius,  cy },
                    { cx,              cy + sumRadius },
                    { cx - sumRadius,  cy }, outCol, 1.8f);

                // ツールチップ
                if (hovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::BeginTooltip();
                    ImGui::Text("frame=%d  (概要: 全ボーン一括移動)", key.frame);
                    ImGui::EndTooltip();

                    // ドラッグ開始
                    if (ImGui::IsMouseClicked(0) && !drag_.active) {
                        drag_ = { trackIdx, ki, true, DragState::Mode::Move,
                                  key.frame, 0, ImGui::GetMousePos().x };
                        key.selected = true;
                    }
                }
            }
            return changed;
        }
        // =====================================================================
        // 通常トラック（既存処理）
        // =====================================================================

        // ドラッグ終了
        if (drag_.active && drag_.trackIdx == trackIdx && !ImGui::IsMouseDown(0)) {
            const int endFrame = (drag_.keyIdx < static_cast<int>(track.keys.size()))
                ? track.keys[drag_.keyIdx].frame : drag_.startFrame;
            const int endDur = (drag_.keyIdx < static_cast<int>(track.keys.size()))
                ? track.keys[drag_.keyIdx].duration : drag_.startDuration;

            // 実際に動いたときだけ changed = true
            if (endFrame != drag_.startFrame || endDur != drag_.startDuration) {
                track.SortKeys();
                if (onMoveKey_ && drag_.keyIdx < static_cast<int>(track.keys.size()))
                    onMoveKey_(trackIdx, drag_.keyIdx, endFrame);
                changed = true;
            }
            drag_.active = false;
        }

        int removeIdx = -1;
        for (int ki = 0; ki < static_cast<int>(track.keys.size()); ++ki) {
            DopeKey& key = track.keys[ki];
            ImU32    fillCol = ResolveKeyColor(track, key, false);
            bool     keyChanged = false;

            if (key.shape == KeyShape::Bar || key.duration > 0) {
                DrawKeyBar(dl, rowMin, cellW, key, trackIdx, ki, fillCol, totalFrames);
            }
            else {
                keyChanged = DrawKey(
                    dl,
                    { rowMin.x + key.frame * cellW, rowMin.y + halfH },
                    radius,
                    fillCol, IM_COL32(255, 255, 255, 60),
                    key, trackIdx, ki, cellW, totalFrames);
            }

            if (keyChanged && !track.readOnly) changed = true;

            if (key.selected && ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace) && !track.readOnly)
                removeIdx = ki;
        }

        if (removeIdx >= 0) {
            if (onDeleteKey_) onDeleteKey_(trackIdx, removeIdx);
            else              track.RemoveKey(removeIdx);
            changed = true;
        }

        // 右クリック → 追加/削除ポップアップ（バー上では追加を出さない）
        if (!track.readOnly) {
            ImVec2 trackMax = { rowMin.x + totalFrames * cellW, rowMin.y + kRowH };

            bool hoveringAnyBar = false;
            for (const auto& k : track.keys) {
                if (k.duration > 0) {
                    float bx0 = rowMin.x + k.frame * cellW;
                    float bx1 = rowMin.x + k.EndFrame() * cellW;
                    if (ImGui::IsMouseHoveringRect({ bx0, rowMin.y }, { bx1, rowMin.y + kRowH })) {
                        hoveringAnyBar = true; break;
                    }
                }
            }

            if (!hoveringAnyBar
                && ImGui::IsMouseHoveringRect(rowMin, trackMax)
                && ImGui::IsMouseClicked(1)) {
                pendingFrame_ = std::clamp(
                    static_cast<int>((ImGui::GetMousePos().x - rowMin.x) / cellW),
                    0, totalFrames);
                pendingValue_ = 0.0f;

                switch (track.type) {
                case TrackType::AttackHitbox:
                case TrackType::InvincibleFrame:
                case TrackType::ArmorFrame:
                case TrackType::ComboWindow:
                case TrackType::CancelWindow:
                case TrackType::CounterWindow:
                    pendingDuration_ = 10; // 区間を扱うものはデフォルトでバーにする
                    break;
                default:
                    pendingDuration_ = 0;  // それ以外（Motion系など）はひし形にする
                    break;
                }
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
        float cellW, int totalFrames) {
        bool  changed = false;
        float cx = center.x, cy = center.y;

        if (key.selected) outlineCol = IM_COL32(255, 255, 255, 200);

        switch (key.shape) {
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

        if (hovered) {
            dl->AddQuad(
                { cx, cy - radius - 2 }, { cx + radius + 2, cy },
                { cx, cy + radius + 2 }, { cx - radius - 2, cy },
                IM_COL32(255, 255, 255, 120), 1.5f);

            ImGui::BeginTooltip();
            ImGui::Text("frame=%d  value=%.3f  sub=%d", key.frame, key.value, key.subType);
            if (!key.tag.empty()) ImGui::Text("tag: %s", key.tag.c_str());
            ImGui::EndTooltip();

            if (ImGui::IsMouseClicked(0) && !drag_.active) {
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::Move,
                          key.frame, 0, ImGui::GetMousePos().x };
                selectionRequests_.push_back({ trackIdx, keyIdx });
                key.selected = true;
                if (onSelectKey_) onSelectKey_(trackIdx, keyIdx, true);
            }
        }

        if (drag_.active && drag_.trackIdx == trackIdx && drag_.keyIdx == keyIdx
            && ImGui::IsMouseDown(0)) {
            int newFrame = std::clamp(
                drag_.startFrame + static_cast<int>(
                    (ImGui::GetMousePos().x - drag_.startMouseX) / cellW),
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
        ImU32 fillCol, int totalFrames) {
        const float pad = 2.0f;
        const float edgeW = 6.0f;

        float x0 = rowMin.x + key.frame * cellW + pad;
        float x1 = rowMin.x + key.EndFrame() * cellW - pad;
        float y0 = rowMin.y + pad;
        float y1 = rowMin.y + kRowH - pad;
        if (x1 <= x0) x1 = x0 + 4.0f;

        ImU32 outCol = key.selected
            ? IM_COL32(255, 255, 255, 200)
            : IM_COL32(255, 255, 255, 80);

        // ── 描画 ──
        dl->AddRectFilled({ x0, y0 }, { x1, y1 }, fillCol, 3.0f);
        dl->AddRect({ x0, y0 }, { x1, y1 }, outCol, 3.0f, 0, 1.5f);

        if ((x1 - x0) > 20.0f) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%df", key.duration);
            dl->AddText({ x0 + 4, y0 + 3 }, IM_COL32(255, 255, 255, 200), buf);
        }

        // ── リサイズハンドル ──
        ImVec2 leftMin = { x0,         y0 };
        ImVec2 leftMax = { x0 + edgeW, y1 };
        ImVec2 rightMin = { x1 - edgeW, y0 };
        ImVec2 rightMax = { x1,         y1 };

        bool hovLeft = ImGui::IsMouseHoveringRect(leftMin, leftMax);
        bool hovRight = ImGui::IsMouseHoveringRect(rightMin, rightMax);
        bool hovBar = ImGui::IsMouseHoveringRect({ x0, y0 }, { x1, y1 });

        if (!drag_.active && hovLeft)
            dl->AddRectFilled(leftMin, leftMax, IM_COL32(255, 255, 255, 60), 2.0f);
        if (!drag_.active && hovRight)
            dl->AddRectFilled(rightMin, rightMax, IM_COL32(255, 255, 255, 60), 2.0f);

        // ── カーソル変更 ──
        if (!drag_.active) {
            if (hovLeft || hovRight)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            else if (hovBar)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // ── ツールチップ ──
        if (hovBar && !drag_.active) {
            ImGui::BeginTooltip();
            ImGui::Text("start=%d  end=%d  dur=%d",
                key.frame, key.EndFrame(), key.duration);
            if (!key.tag.empty()) ImGui::Text("tag: %s", key.tag.c_str());
            ImGui::EndTooltip();
        }

        //// ── 右クリック削除メニュー ──
        //const std::string popupId = "##DeleteBar_"
        //    + std::to_string(trackIdx) + "_" + std::to_string(keyIdx);

        //if (hovBar && ImGui::IsMouseClicked(1) && !drag_.active)
        //    ImGui::OpenPopup(popupId.c_str());

        //if (ImGui::BeginPopup(popupId.c_str())) {
        //    ImGui::Text("frame=%d  dur=%df", key.frame, key.duration);
        //    ImGui::Separator();
        //    if (ImGui::MenuItem("削除")) {
        //        if (onDeleteKey_) onDeleteKey_(trackIdx, keyIdx);
        //        else              track.RemoveKey(keyIdx);
        //        ImGui::CloseCurrentPopup();
        //    }
        //    ImGui::EndPopup();
        //}

        // ── ドラッグ開始 ──
        if (!drag_.active && ImGui::IsMouseClicked(0)) {
            if (hovLeft)
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::ResizeLeft,
                          key.frame, key.duration, ImGui::GetMousePos().x };
            else if (hovRight)
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::ResizeRight,
                          key.frame, key.duration, ImGui::GetMousePos().x };
            else if (hovBar) {
                drag_ = { trackIdx, keyIdx, true, DragState::Mode::Move,
                          key.frame, key.duration, ImGui::GetMousePos().x };
                if (onSelectKey_) onSelectKey_(trackIdx, keyIdx, true);
            }
            if (drag_.active) {
                selectionRequests_.push_back({ trackIdx, keyIdx });
                key.selected = true;
            }
        }

        // ── ドラッグ中の更新 ──
        if (drag_.active && drag_.trackIdx == trackIdx && drag_.keyIdx == keyIdx
            && ImGui::IsMouseDown(0)) {
            int delta = static_cast<int>(
                (ImGui::GetMousePos().x - drag_.startMouseX) / cellW);

            switch (drag_.mode) {
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
        ImDrawList* dl, ImVec2 origin, float timelineH, int totalFrames) {
        if (seekFrame_ < 0 || seekFrame_ > totalFrames) return;

        float sx = origin.x + seekFrame_ * zoomX_;  // kLabelW オフセット不要

        dl->AddTriangleFilled(
            { sx - 5, origin.y }, { sx + 5, origin.y }, { sx, origin.y + 10 },
            IM_COL32(255, 220, 50, 230));
        dl->AddLine(
            { sx, origin.y + 10 }, { sx, origin.y + timelineH },
            IM_COL32(255, 220, 50, 180), 1.5f);

        ImVec2 hitMin = { sx - 6, origin.y };
        ImVec2 hitMax = { sx + 6, origin.y + 12 };
        bool   hovered = ImGui::IsMouseHoveringRect(hitMin, hitMax);

        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        static bool seekDragging = false;
        if (hovered && ImGui::IsMouseClicked(0)) seekDragging = true;

        if (seekDragging) {
            if (ImGui::IsMouseDown(0)) {
                float relX = ImGui::GetMousePos().x - origin.x;
                int   newFrame = std::clamp(
                    static_cast<int>(relX / zoomX_), 0, totalFrames);
                if (newFrame != seekFrame_) {
                    seekFrame_ = newFrame;
                    if (onSeek_) onSeek_(seekFrame_);
                }
            }
            else {
                seekDragging = false;
            }
        }
    }
#endif

    //=============================================================================
    // キー追加ポップアップ
    //=============================================================================
#ifdef USE_IMGUI
    void DopeSheetEditor::DrawAddKeyPopup(std::vector<DopeTrack>& tracks) {
        if (!ImGui::BeginPopup("##DopeAddKey")) return;

        const bool  valid = (pendingTrackIdx_ >= 0 &&
            pendingTrackIdx_ < static_cast<int>(tracks.size()));
        const char* label = valid ? tracks[pendingTrackIdx_].label.c_str() : "?";

        ImGui::Text("キー追加  [%s]  frame=%d", label, pendingFrame_);
        ImGui::Separator();
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("値", &pendingValue_, 0.1f);
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("持続(f)", &pendingDuration_);
        pendingDuration_ = std::max(0, pendingDuration_);
        ImGui::Separator();

        if (ImGui::Button("追加") && valid) {
            if (onAddKey_) {
                onAddKey_(pendingTrackIdx_, pendingFrame_, pendingValue_);
            }
            else {
                auto& t = tracks[pendingTrackIdx_];
                if (!t.readOnly)
                    t.AddKey(pendingFrame_, pendingValue_, 0, pendingDuration_);
            }
            pendingValue_ = 0.0f;
            pendingDuration_ = 0;
            showAddPopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) {
            showAddPopup_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
#endif

    //=============================================================================
    // キー色の解決
    //=============================================================================
#ifdef USE_IMGUI
    ImU32 DopeSheetEditor::ResolveKeyColor(
        const DopeTrack& track, const DopeKey& key, bool hovered) const {
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