#ifdef USE_IMGUI

#include "AttackCameraEditor.h"
#include "PlayerCamera.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <string>

// ============================================================
// 初期化
// ============================================================
void AttackCameraEditor::Initialize(AttackCameraComponent* component, PlayerCamera* playerCamera) {
    component_ = component;
    playerCamera_ = playerCamera;
}

// ============================================================
// メイン描画
// ============================================================
void AttackCameraEditor::DrawImGui() {
    if (!component_) return;

    ImGui::PushID("AttackCameraEditor");

    // ツールバー（保存・読み込み）
    DrawToolbar();
    ImGui::Separator();

    // 左: ワーク一覧 / 右: エディター
    ImGui::Columns(2, "acw_columns");
    ImGui::SetColumnWidth(0, 200.0f);

    DrawWorkList();

    ImGui::NextColumn();

    if (selectedWorkIdx_ >= 0 &&
        selectedWorkIdx_ < static_cast<int>(component_->GetWorks().size())) {
        DrawWorkSettings();
        ImGui::Separator();
        DrawKeyframeTimeline();
        ImGui::Separator();
        DrawKeyframeInspector();
        ImGui::Separator();
        DrawPreviewControls();
    }
    else {
        ImGui::TextDisabled("← ワークを選択してください");
    }

    ImGui::Columns(1);
    ImGui::PopID();
}

// ============================================================
// ツールバー
// ============================================================
void AttackCameraEditor::DrawToolbar() {
    if (ImGui::Button("保存")) {
        component_->SaveToFile(filePath_);
    }
    ImGui::SameLine();
    if (ImGui::Button("読み込み")) {
        component_->LoadFromFile(filePath_);
        selectedWorkIdx_ = -1;
        selectedKfIdx_ = -1;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    char pathBuf[256];
    ::strncpy_s(pathBuf, filePath_.c_str(), sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = '\0';
    if (ImGui::InputText("##path", pathBuf, sizeof(pathBuf)))
        filePath_ = pathBuf;
}

// ============================================================
// ワーク一覧
// ============================================================
void AttackCameraEditor::DrawWorkList() {
    ImGui::Text("カメラワーク一覧");
    ImGui::Separator();

    auto& works = component_->GetWorks();
    for (int i = 0; i < static_cast<int>(works.size()); ++i) {
        bool selected = (i == selectedWorkIdx_);
        if (ImGui::Selectable(works[i].name.c_str(), selected)) {
            selectedWorkIdx_ = i;
            selectedKfIdx_ = -1;
        }
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##newName", newNameBuf_, sizeof(newNameBuf_));
    ImGui::SameLine();
    if (ImGui::Button("追加") && newNameBuf_[0] != '\0') {
        AttackCameraWork w;
        w.name = newNameBuf_;
        component_->AddWork(w);
        std::memset(newNameBuf_, 0, sizeof(newNameBuf_));
    }

    if (selectedWorkIdx_ >= 0 && selectedWorkIdx_ < static_cast<int>(works.size())) {
        if (ImGui::Button("削除")) {
            component_->RemoveWork(works[selectedWorkIdx_].name);
            selectedWorkIdx_ = -1;
            selectedKfIdx_ = -1;
        }
    }
}

// ============================================================
// ワーク設定（補間含む）
// ============================================================
void AttackCameraEditor::DrawWorkSettings() {
    auto& work = component_->GetWorks()[selectedWorkIdx_];

    ImGui::Text("ワーク: %s", work.name.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("##dur", &work.totalDuration, 0.01f, 0.05f, 10.0f, "%.2fs");

    // ============================================================
    // 開始時補間の設定
    // ============================================================
    ImGui::TextDisabled("開始時補間");
    ImGui::Checkbox("開始時補間を使用", &work.useStartInterpolation);
    if (work.useStartInterpolation) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::DragFloat("##startInterp", &work.startInterpolationDuration,
            0.01f, 0.0f, 2.0f, "%.2fs");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play() 時の Idle → Playing への補間時間");
    }

    ImGui::Separator();

    // ============================================================
    // 終了時補間の設定
    // ============================================================
    ImGui::TextDisabled("終了時補間");
    ImGui::Checkbox("終了時リセット", &work.resetOnFinish);

    if (work.resetOnFinish) {
        ImGui::Checkbox("終了時補間を使用##useReturn", &work.useReturnInterpolation);
        if (work.useReturnInterpolation) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::DragFloat("##ret", &work.returnDuration, 0.01f, 0.0f, 2.0f, "%.2fs");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("演出終了後、元の状態へ補間で戻る時間\n0 = 瞬間切り替え");
        }
    }

    ImGui::Separator();

    // ============================================================
    // フレーミング設定
    // ============================================================
    ImGui::TextDisabled("フレーミング");
    ImGui::Checkbox("プレイヤーをフレーム内に保持##keepInFrame", &work.keepPlayerInFrame);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("再生中、プレイヤーが画角外へ出ないよう引き戻す\nOFF = カットシーン的に画角外を許可");
}

// ============================================================
// キーフレーム タイムライン
// ============================================================
void AttackCameraEditor::DrawKeyframeTimeline() {
    auto& work = component_->GetWorks()[selectedWorkIdx_];

    ImGui::Text("キーフレーム タイムライン");

    // ---- タイムライン描画 ----
    const float tlW = ImGui::GetContentRegionAvail().x - 16.0f;
    const float tlH = 36.0f;
    const float kfR = 8.0f;

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景
    dl->AddRectFilled(origin, { origin.x + tlW, origin.y + tlH },
        IM_COL32(30, 30, 30, 255), 4.0f);
    dl->AddRect(origin, { origin.x + tlW, origin.y + tlH },
        IM_COL32(80, 80, 80, 255), 4.0f);

    // 再生カーソル
    if (isPreviewPlaying_ && component_->IsActive()) {
        float ratio = component_->GetPlayRatio();
        float px = origin.x + ratio * tlW;
        ImU32 cursorCol = component_->IsReturning()
            ? IM_COL32(255, 200, 0, 220)   // 黄: 復帰中
            : IM_COL32(255, 100, 0, 220);  // 橙: 再生中
        dl->AddLine({ px, origin.y }, { px, origin.y + tlH }, cursorCol, 2.0f);
    }

    // returnDuration ゾーン（totalDuration 以降をオレンジ帯で表示）
    if (work.resetOnFinish && work.useReturnInterpolation && work.returnDuration > 0.0f) {
        ImVec2 retStart = { origin.x + tlW, origin.y };
        float  retW = std::min(work.returnDuration / work.totalDuration * tlW, 60.0f);
        ImVec2 retEnd = { retStart.x + retW, origin.y + tlH };
        dl->AddRectFilled(retStart, retEnd, IM_COL32(255, 180, 0, 60));
        dl->AddLine(retStart, { retStart.x, origin.y + tlH }, IM_COL32(255, 200, 50, 200), 1.5f);
    }

    // キーフレームのマーカー
    float cy = origin.y + tlH * 0.5f;
    for (int i = 0; i < static_cast<int>(work.keyframes.size()); ++i) {
        auto& kf = work.keyframes[i];
        float t = (work.totalDuration > 0.0f) ? kf.time / work.totalDuration : 0.0f;
        float kx = origin.x + t * tlW;
        bool  sel = (i == selectedKfIdx_);
        ImU32 col = sel ? IM_COL32(255, 200, 0, 255) : IM_COL32(0, 180, 255, 255);

        dl->AddCircleFilled({ kx, cy }, kfR, col);
        dl->AddCircle({ kx, cy }, kfR, IM_COL32(255, 255, 255, 120));

        // クリック選択
        ImGui::SetCursorScreenPos({ kx - kfR, cy - kfR });
        ImGui::InvisibleButton(("kf_" + std::to_string(i)).c_str(),
            { kfR * 2.0f, kfR * 2.0f });
        if (ImGui::IsItemClicked()) selectedKfIdx_ = i;

        // ドラッグで時刻変更
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float dx = ImGui::GetIO().MouseDelta.x / tlW;
            kf.time = std::clamp(kf.time + dx * work.totalDuration,
                0.0f, work.totalDuration);
            // タイムで昇順ソート（選択インデックスも追従）
            std::sort(work.keyframes.begin(), work.keyframes.end(),
                [](const AttackCameraKeyframe& a, const AttackCameraKeyframe& b) {
                    return a.time < b.time;
                });
        }
    }

    ImGui::Dummy({ tlW, tlH });

    // キーフレーム追加
    ImGui::SameLine();
    if (ImGui::SmallButton("+KF")) {
        AttackCameraKeyframe kf;
        kf.time = work.totalDuration * 0.5f;
        work.keyframes.push_back(kf);
        std::sort(work.keyframes.begin(), work.keyframes.end(),
            [](const AttackCameraKeyframe& a, const AttackCameraKeyframe& b) {
                return a.time < b.time;
            });
        selectedKfIdx_ = -1;
    }
    if (selectedKfIdx_ >= 0 &&
        selectedKfIdx_ < static_cast<int>(work.keyframes.size())) {
        ImGui::SameLine();
        if (ImGui::SmallButton("-KF")) {
            work.keyframes.erase(work.keyframes.begin() + selectedKfIdx_);
            selectedKfIdx_ = -1;
        }
    }
}

// ============================================================
// 選択キーフレームのインスペクター
// ============================================================
void AttackCameraEditor::DrawKeyframeInspector() {
    if (selectedKfIdx_ < 0) {
        ImGui::TextDisabled("キーフレームを選択してください");
        return;
    }
    auto& work = component_->GetWorks()[selectedWorkIdx_];
    if (selectedKfIdx_ >= static_cast<int>(work.keyframes.size())) return;

    auto& kf = work.keyframes[selectedKfIdx_];
    ImGui::Text("キーフレーム [%d]  (t = %.3f s)", selectedKfIdx_, kf.time);

    ImGui::DragFloat("時刻 (s)", &kf.time, 0.005f, 0.0f, work.totalDuration);

    ImGui::Separator();
    ImGui::TextDisabled("位置 / 方向（カメラローカル空間）");
    ImGui::DragFloat3("位置オフセット", &kf.posOffset.x, 0.05f);
    ImGui::DragFloat3("回転オフセット", &kf.rotOffset.x, 0.005f, -1.57f, 1.57f,
        "P:%.3f Y:%.3f R:%.3f");
    ImGui::DragFloat("FOV 差分 (rad)", &kf.fovDelta, 0.005f, -0.5f, 0.5f);

    ImGui::Separator();
    ImGui::TextDisabled("シェイク（0 = なし）");
    ImGui::DragFloat("強さ", &kf.shakeIntensity, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("持続時間 (s)", &kf.shakeDuration, 0.01f, 0.0f, 2.0f);

    ImGui::Separator();
    ImGui::TextDisabled("タイムスケール（1.0 = 通常 / 0.3 = スロー）");
    ImGui::DragFloat("タイムスケール", &kf.timeScale, 0.01f, 0.05f, 2.0f);

    // 時刻でソート
    std::sort(work.keyframes.begin(), work.keyframes.end(),
        [](const AttackCameraKeyframe& a, const AttackCameraKeyframe& b) {
            return a.time < b.time;
        });
}

// ============================================================
// プレビュー制御
// ============================================================
void AttackCameraEditor::DrawPreviewControls() {
    if (!playerCamera_) return;

    auto& work = component_->GetWorks()[selectedWorkIdx_];

    if (!isPreviewPlaying_) {
        if (ImGui::Button("プレビュー")) {
            playerCamera_->PlayAttackCameraWork(work.name);
            isPreviewPlaying_ = true;
        }
    }
    else {
        if (component_->IsReturning()) {
            ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "復帰補間中...");
        }
        else {
            ImGui::TextColored({ 0.3f, 1.0f, 0.3f, 1.0f }, "再生中...");
        }
        ImGui::SameLine();
        if (ImGui::Button("停止")) {
            playerCamera_->StopAttackCameraWork();
            isPreviewPlaying_ = false;
        }
        // 自動終了検知（Active になったら終了）
        if (!component_->IsActive()) {
            isPreviewPlaying_ = false;
        }
    }
}

#endif // USE_IMGUI