#include "SpeedCurvePanel.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

#include "../MotionEditorContext.h"
#include "../../Core/Motion.h"

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------
void SpeedCurvePanel::Initialize(MotionEditorContext* context)
{
    context_ = context;
}

// -----------------------------------------------------------------------
// DrawCurveEditor  カスタムカーブエディタ（戻り値: dirty になったか）
//
// 操作:
//   ・左ドラッグ     : 点を移動（端点は X 固定）
//   ・右クリック(空白): 点を追加
//   ・右クリック(点上): コンテキストメニュー → 削除
// -----------------------------------------------------------------------
#ifdef USE_IMGUI
bool SpeedCurvePanel::DrawCurveEditor(ImVec2 size)
{
    bool dirty = false;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      rMin = ImGui::GetCursorScreenPos();
    ImVec2      rMax = { rMin.x + size.x, rMin.y + size.y };
    float       w = size.x;
    float       h = size.y;

    // 不可視のヒットエリア（マウスイベント取得用）
    ImGui::InvisibleButton("##curve_area", size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool leftClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    ImVec2 mousePos = ImGui::GetMousePos();

    // ================================================================
    // 背景
    // ================================================================
    dl->AddRectFilled(rMin, rMax, IM_COL32(28, 28, 32, 255), 4.0f);
    dl->AddRect(rMin, rMax, IM_COL32(80, 80, 90, 200), 4.0f);

    float yMin = delegate_.boundsMinY;
    float yMax = delegate_.boundsMaxY;
    float yRange = yMax - yMin;

    // ================================================================
    // グリッド描画
    // ================================================================
    {
        // --- 縦グリッド（X軸方向: 0.25刻み） ---
        const float xSteps[] = { 0.25f, 0.5f, 0.75f };
        for (float xs : xSteps) {
            float px = rMin.x + xs * w;
            dl->AddLine({ px, rMin.y }, { px, rMax.y },
                (xs == 0.5f) ? IM_COL32(90, 90, 110, 180)
                : IM_COL32(60, 60, 70, 140),
                (xs == 0.5f) ? 1.0f : 0.8f);
        }

        // --- 横グリッド（Y軸方向: 0.5 刻み, 1.0 は強調） ---
        float yStep = 0.5f;
        for (float y = yMin; y <= yMax + 1e-4f; y += yStep) {
            float py = rMax.y - (y - yMin) / yRange * h;
            bool  is1 = (std::abs(y - 1.0f) < 0.01f);
            dl->AddLine({ rMin.x, py }, { rMax.x, py },
                is1 ? IM_COL32(120, 220, 120, 160)   // Y=1.0 は緑で強調
                : IM_COL32(60, 60, 70, 130),
                is1 ? 1.2f : 0.8f);

            // Y軸ラベル
            char buf[8];
            snprintf(buf, sizeof(buf), "%.1f", y);
            dl->AddText({ rMin.x + 3.f, py - 10.f },
                is1 ? IM_COL32(140, 230, 140, 200)
                : IM_COL32(120, 120, 140, 160),
                buf);
        }
    }

    // ================================================================
    // 現在のスクラブ位置の縦線
    // ================================================================
    Motion* motion = context_ ? context_->currentMotion : nullptr;
    if (motion && motion->GetDuration() > 0.0f) {
        float nt = std::clamp(context_->scrubTime / motion->GetDuration(), 0.0f, 1.0f);
        float spx = rMin.x + nt * w;
        dl->AddLine({ spx, rMin.y }, { spx, rMax.y },
            IM_COL32(80, 200, 255, 100), 1.0f);
    }

    // ================================================================
    // カーブ描画（折れ線）
    // ================================================================
    {
        const int segs = static_cast<int>(w);  // ピクセル単位で補間
        if (delegate_.points.size() >= 2) {
            ImVec2 prev = delegate_.ToScreen(delegate_.points.front(), rMin, rMax);
            for (int i = 1; i <= segs; ++i) {
                float x = static_cast<float>(i) / segs;
                float y = delegate_.Evaluate(x);
                ImVec2 cur = delegate_.ToScreen({ x, y }, rMin, rMax);
                dl->AddLine(prev, cur, IM_COL32(68, 210, 255, 230), 2.0f);
                prev = cur;
            }
        }
    }

    // ================================================================
    // 焼き込み前スナップショットオーバーレイ
    // ================================================================
    if (hasBakedSnapshot_ && showBakedPreview_) {
        DrawBakedPreviewOverlay(rMin, rMax);
    }

    // ================================================================
    // ドラッグ操作（左ボタン）
    // ================================================================
    if (hovered && leftClick && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        int hit = delegate_.HitTest(mousePos, rMin, rMax);
        if (hit >= 0) {
            dragIndex_ = hit;
            dragging_ = true;
        }
    }
    if (dragging_) {
        if (leftDown) {
            ImVec2 local = delegate_.ToLocal(mousePos, rMin, rMax);
            local = delegate_.Clamp(local);

            // 端点は X を固定
            if (dragIndex_ == 0)
                local.x = 0.0f;
            else if (dragIndex_ == static_cast<int>(delegate_.points.size()) - 1)
                local.x = 1.0f;
            else {
                // 隣接点を追い越さないよう X をクランプ
                float xPrev = delegate_.points[dragIndex_ - 1].x + 0.001f;
                float xNext = delegate_.points[dragIndex_ + 1].x - 0.001f;
                local.x = std::clamp(local.x, xPrev, xNext);
            }
            delegate_.points[dragIndex_] = local;
            dirty = true;
        }
        else {
            dragging_ = false;
            dragIndex_ = -1;
        }
    }

    // ================================================================
    // 右クリック処理
    // ================================================================
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        rightClickIdx_ = delegate_.HitTest(mousePos, rMin, rMax);
        rightClickLocalPos_ = delegate_.ToLocal(mousePos, rMin, rMax);

        // 点の上ならコンテキストメニュー、空白なら即追加
        if (rightClickIdx_ >= 0) {
            ImGui::OpenPopup("##CurveCtx");
        }
        else {
            // 空白クリック → 点を追加
            if (delegate_.AddPoint(rightClickLocalPos_)) {
                dirty = true;
            }
        }
    }

    if (ImGui::BeginPopup("##CurveCtx")) {
        // 端点は削除不可
        bool isEndpoint = (rightClickIdx_ == 0 ||
            rightClickIdx_ == static_cast<int>(delegate_.points.size()) - 1);

        if (isEndpoint) {
            ImGui::TextDisabled("端点は削除できません");
        }
        else {
            if (ImGui::MenuItem("\uf00d  この点を削除")) {
                delegate_.RemovePoint(rightClickIdx_);
                dirty = true;
                rightClickIdx_ = -1;
            }
        }

        // 値の手動入力欄
        if (rightClickIdx_ >= 0 && rightClickIdx_ < static_cast<int>(delegate_.points.size())) {
            ImGui::Separator();
            ImGui::Text("X = %.3f", delegate_.points[rightClickIdx_].x);
            ImGui::SetNextItemWidth(100.f);
            float editY = delegate_.points[rightClickIdx_].y;
            if (ImGui::DragFloat("倍率##ctxY", &editY, 0.01f,
                delegate_.boundsMinY, delegate_.boundsMaxY, "%.2fx")) {
                delegate_.points[rightClickIdx_].y = editY;
                dirty = true;
            }
        }
        ImGui::EndPopup();
    }

    // ================================================================
    // 制御点描画
    // ================================================================
    for (int i = 0; i < static_cast<int>(delegate_.points.size()); ++i) {
        ImVec2 sp = delegate_.ToScreen(delegate_.points[i], rMin, rMax);
        bool   isEnd = (i == 0 || i == static_cast<int>(delegate_.points.size()) - 1);
        bool   isDrag = (dragging_ && dragIndex_ == i);

        // 影（立体感）
        dl->AddCircleFilled(sp, 7.5f, IM_COL32(0, 0, 0, 120));

        // 本体
        ImU32 fillCol = isDrag ? IM_COL32(255, 230, 80, 255) :
            isEnd ? IM_COL32(80, 200, 255, 255) :
            IM_COL32(240, 240, 240, 255);
        dl->AddCircleFilled(sp, 6.0f, fillCol);

        // 枠線
        dl->AddCircle(sp, 6.0f,
            isDrag ? IM_COL32(255, 255, 100, 255)
            : IM_COL32(60, 160, 220, 200),
            0, 1.5f);

        // ホバー時の値ツールチップ
        if (hovered) {
            float dx = mousePos.x - sp.x, dy = mousePos.y - sp.y;
            if (dx * dx + dy * dy < 10.f * 10.f) {
                ImGui::SetTooltip("t=%.3f  speed=%.3f x",
                    delegate_.points[i].x, delegate_.points[i].y);
            }
        }
    }

    // ================================================================
    // X軸ラベル（下端）
    // ================================================================
    {
        const float yLabelY = rMax.y + 3.0f;
        const ImU32 lblCol = IM_COL32(160, 160, 180, 200);
        dl->AddText({ rMin.x,              yLabelY }, lblCol, "0");
        dl->AddText({ rMin.x + w * 0.25f - 4,  yLabelY }, lblCol, "0.25");
        dl->AddText({ rMin.x + w * 0.5f - 6, yLabelY }, lblCol, "0.5");
        dl->AddText({ rMin.x + w * 0.75f - 4,  yLabelY }, lblCol, "0.75");
        dl->AddText({ rMin.x + w - 8,      yLabelY }, lblCol, "1");
    }

    // X軸ラベル分のスペース確保
    ImGui::Dummy({ 0, 14.0f });

    return dirty;
}
#endif

// -----------------------------------------------------------------------
// DrawImGui
// -----------------------------------------------------------------------
void SpeedCurvePanel::DrawImGui()
{
#ifdef USE_IMGUI
    if (!context_) return;

    Motion* motion = context_->currentMotion;

    // モーションが切り替わったら制御点を再取得
    if (motion != lastMotion_) {
        lastMotion_ = motion;
        hasBakedSnapshot_ = false;
        showBakedPreview_ = false;
        PullFromMotion();
        isDirty_ = false;
    }

    ImGui::PushID("SpeedCurvePanel");

    // ============================================================
    // CollapsingHeader
    // ============================================================
    char headerLabel[128];
    if (hasBakedSnapshot_)
        snprintf(headerLabel, sizeof(headerLabel),
            "\uf0e7 タイムスケールカーブ  [焼込済]###SpeedCurveHeader");
    else if (isDirty_)
        snprintf(headerLabel, sizeof(headerLabel),
            "\uf0e7 タイムスケールカーブ  *###SpeedCurveHeader");
    else
        snprintf(headerLabel, sizeof(headerLabel),
            "\uf0e7 タイムスケールカーブ###SpeedCurveHeader");

    bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen);

    if (!open) {
        if (motion && motion->GetDuration() > 0.0f && ImGui::IsItemHovered()) {
            float nt = std::clamp(context_->scrubTime / motion->GetDuration(), 0.0f, 1.0f);
            float spd = motion->EvaluateSpeedCurve(nt);
            ImGui::SetTooltip("現在速度倍率: %.2f x", spd);
        }
        ImGui::PopID();
        return;
    }

    // ============================================================
    // モーション未選択
    // ============================================================
    if (!motion) {
        ImGui::TextDisabled("モーションが選択されていません");
        ImGui::PopID();
        return;
    }

    // ============================================================
    // 速度上限スライダ
    // ============================================================
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderFloat("速度上限##maxspd", &maxSpeedEdit_, 1.0f, 8.0f, "%.1fx")) {
        delegate_.SetMaxSpeed(maxSpeedEdit_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(1.0=等速)");

    if (hasBakedSnapshot_) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 130.0f);
        ImGui::Checkbox("\uf0c7 焼込前を表示##bkprev", &showBakedPreview_);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("焼き込む前のカーブをオレンジ色で重ねて表示します");
    }

    // ============================================================
    // 操作ヒント（エディタ上部に1行）
    // ============================================================
    ImGui::TextDisabled("右クリック(空白):点を追加  右クリック(点上):削除メニュー  左ドラッグ:移動");

    // ============================================================
    // カスタムカーブエディタ本体
    // ============================================================
    ImVec2 curveSize = { ImGui::GetContentRegionAvail().x, 180.0f };
    if (DrawCurveEditor(curveSize)) {
        isDirty_ = true;
    }

    ImGui::Separator();

    // ============================================================
    // ボタン行
    // ============================================================

    // --- ランタイム適用 ---
    {
        const bool dis = !isDirty_;
        if (dis) ImGui::BeginDisabled();
        if (ImGui::Button("\uf0e7 適用##apply", ImVec2(80, 0))) {
            PushToMotion();
            isDirty_ = false;
            context_->statusMsg = "スピードカーブをランタイム適用しました";
        }
        if (dis) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("カーブをMotionに反映します（キーフレームは変更しません）");
    }

    ImGui::SameLine(0, 6);

    // --- 焼き込み ---
    if (ImGui::Button("\uf0c7 Bake##bake", ImVec2(80, 0))) {
        bakeConfirmPending_ = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("カーブをキーフレーム時間軸に積分して焼き込みます\n（不可逆操作）");

    if (bakeConfirmPending_) {
        ImGui::OpenPopup("##BakeConfirm");
        bakeConfirmPending_ = false;
    }
    if (ImGui::BeginPopupModal("##BakeConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("カーブをキーフレームに焼き込みます。");
        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "この操作は元に戻せません。");
        ImGui::Spacing();
        if (ImGui::Button("実行##bakeok", ImVec2(80, 0))) {
            bakedSnapshot_ = delegate_.points;
            hasBakedSnapshot_ = true;
            showBakedPreview_ = true;

            PushToMotion();
            BakeSpeedCurve();
            PullFromMotion();
            isDirty_ = false;
            context_->statusMsg = "スピードカーブを焼き込みました";
            context_->requireTimelineRebuild = true;
            context_->lastAppliedScrubTime = -1.0f;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 8);
        if (ImGui::Button("キャンセル##bakecancel", ImVec2(80, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine(0, 6);

    // --- リセット ---
    if (ImGui::Button("\uf0e2 リセット##reset", ImVec2(80, 0))) {
        motion->ClearSpeedCurve();
        PullFromMotion();
        isDirty_ = false;
        hasBakedSnapshot_ = false;
        showBakedPreview_ = false;
        context_->statusMsg = "スピードカーブをリセットしました";
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("カーブを等速（直線）に戻します");

    // ============================================================
    // 現在位置の速度プレビュー（右寄せ）
    // ============================================================
    if (motion->GetDuration() > 0.0f) {
        float nt = std::clamp(context_->scrubTime / motion->GetDuration(), 0.0f, 1.0f);
        float spd = motion->EvaluateSpeedCurve(nt);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 140.0f);
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f),
            "現在: %.2fx  (t=%.2f)", spd, nt);
    }

    ImGui::PopID();
#endif
}

// -----------------------------------------------------------------------
// DrawBakedPreviewOverlay
// -----------------------------------------------------------------------
#ifdef USE_IMGUI
void SpeedCurvePanel::DrawBakedPreviewOverlay(ImVec2 rMin, ImVec2 rMax) const
{
    if (bakedSnapshot_.size() < 2) return;

    float w = rMax.x - rMin.x;
    float h = rMax.y - rMin.y;
    float yMin = delegate_.boundsMinY;
    float yMax = delegate_.boundsMaxY;
    float yRange = yMax - yMin;
    if (yRange < 1e-6f || w < 1.0f || h < 1.0f) return;

    auto toScreen = [&](ImVec2 p) -> ImVec2 {
        return {
            rMin.x + std::clamp(p.x, 0.0f, 1.0f) * w,
            rMax.y - (std::clamp(p.y, yMin, yMax) - yMin) / yRange * h
        };
        };

    auto* dl = ImGui::GetWindowDrawList();
    const ImU32 lineCol = IM_COL32(255, 160, 40, 200);
    const ImU32 dotCol = IM_COL32(255, 200, 80, 220);

    // スナップショットを線形補間で描画
    const int segs = static_cast<int>(w);
    auto evalSnap = [&](float x) -> float {
        if (bakedSnapshot_.size() < 2) return 1.0f;
        if (x <= bakedSnapshot_.front().x) return bakedSnapshot_.front().y;
        if (x >= bakedSnapshot_.back().x)  return bakedSnapshot_.back().y;
        for (int i = 0; i + 1 < static_cast<int>(bakedSnapshot_.size()); ++i) {
            if (x <= bakedSnapshot_[i + 1].x) {
                float t = (x - bakedSnapshot_[i].x) /
                    (bakedSnapshot_[i + 1].x - bakedSnapshot_[i].x);
                return bakedSnapshot_[i].y + t * (bakedSnapshot_[i + 1].y - bakedSnapshot_[i].y);
            }
        }
        return 1.0f;
        };

    ImVec2 prev = toScreen({ 0.0f, evalSnap(0.0f) });
    for (int i = 1; i <= segs; ++i) {
        float x = static_cast<float>(i) / segs;
        ImVec2 cur = toScreen({ x, evalSnap(x) });
        dl->AddLine(prev, cur, lineCol, 1.5f);
        prev = cur;
    }

    // 制御点マーカー
    for (const auto& p : bakedSnapshot_)
        dl->AddCircleFilled(toScreen(p), 3.5f, dotCol);

    // 凡例
    dl->AddRectFilled({ rMin.x + 6, rMax.y - 20 },
        { rMin.x + 90, rMax.y - 6 },
        IM_COL32(30, 30, 30, 180), 3.f);
    dl->AddText({ rMin.x + 8, rMax.y - 18 }, dotCol, "Bake\xe5\x89\x8d"); // "Bake前"
}
#endif

// -----------------------------------------------------------------------
// PullFromMotion
// -----------------------------------------------------------------------
#ifdef USE_IMGUI
void SpeedCurvePanel::PullFromMotion()
{
    delegate_.points.clear();
    Motion* motion = context_ ? context_->currentMotion : nullptr;
    if (!motion) return;

    const auto& kfs = motion->GetSpeedCurve().curve.keyframes;
    if (kfs.empty()) {
        delegate_.points = { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
    }
    else {
        for (const auto& kf : kfs)
            delegate_.points.push_back({ kf.time, kf.value });
        delegate_.Sort();
    }
    delegate_.EnsureEndpoints();
}
#endif

// -----------------------------------------------------------------------
// PushToMotion
// -----------------------------------------------------------------------
#ifdef USE_IMGUI
void SpeedCurvePanel::PushToMotion()
{
    Motion* motion = context_ ? context_->currentMotion : nullptr;
    if (!motion) return;

    auto& kfs = motion->GetSpeedCurve().curve.keyframes;
    kfs.clear();
    for (const auto& p : delegate_.points)
        kfs.push_back({ p.x, p.y });

    std::sort(kfs.begin(), kfs.end(),
        [](const Motion::Keyframe<float>& a, const Motion::Keyframe<float>& b) {
            return a.time < b.time;
        });
}
#endif

// -----------------------------------------------------------------------
// BakeSpeedCurve  (元の実装を維持)
// -----------------------------------------------------------------------
void SpeedCurvePanel::BakeSpeedCurve()
{
    Motion* motion = context_ ? context_->currentMotion : nullptr;
    if (!motion || !motion->HasSpeedCurve()) return;

    const float duration = motion->GetDuration();
    if (duration <= 0.0f) return;

    constexpr int   kSamples = 512;
    constexpr float kMinSpeed = 0.05f;

    std::vector<float> invLut(kSamples + 1);
    invLut[0] = 0.0f;
    for (int i = 1; i <= kSamples; ++i) {
        float t0 = static_cast<float>(i - 1) / kSamples;
        float t1 = static_cast<float>(i) / kSamples;
        float s0 = std::max(motion->EvaluateSpeedCurve(t0), kMinSpeed);
        float s1 = std::max(motion->EvaluateSpeedCurve(t1), kMinSpeed);
        invLut[i] = invLut[i - 1] + (1.0f / s0 + 1.0f / s1) * 0.5f / kSamples;
    }

    const float newDurationNorm = invLut[kSamples];
    if (newDurationNorm < 1e-6f) return;
    const float newDuration = newDurationNorm * duration;

    auto remapTime = [&](float T) -> float {
        float normT = std::clamp(T / duration, 0.0f, 1.0f);
        float fIdx = normT * static_cast<float>(kSamples);
        int   lo = static_cast<int>(fIdx);
        int   hi = std::min(lo + 1, kSamples);
        float frac = fIdx - static_cast<float>(lo);
        float newNormT = invLut[lo] + frac * (invLut[hi] - invLut[lo]);
        return newNormT * duration;
        };

    for (auto& [boneName, nodeAnim] : motion->animation_.nodeAnimations_) {
        auto remap = [&](auto& track) {
            for (auto& kf : track.keyframes)
                kf.time = remapTime(kf.time);
            };
        remap(nodeAnim.translate);
        remap(nodeAnim.rotate);
        remap(nodeAnim.scale);
    }

    motion->SetDuration(newDuration);
    motion->ClearSpeedCurve();
}