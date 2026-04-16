// ===========================================================
// VfxMeshEditor.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "VfxMeshEditor.h"
#include "DirectXCommon.h"
#include <PipelineManager/YPipelineManager.h>
#include <imgui.h>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "Debugger/Logger.h"
#include <Loaders/Texture/TextureManager.h>
#include <Editor/Icon/EditorIcon.h>

namespace fs = std::filesystem;

static constexpr size_t kCBVAlignment = 256;
template<typename T>
static constexpr size_t AlignedSize() {
    return (sizeof(T) + kCBVAlignment - 1) & ~(kCBVAlignment - 1);
}

namespace YoRigine {

    VfxMeshEditor::VfxMeshEditor()
    {
        // ---- ランプテクスチャ用 (t1: gTexRamp) ----
        rampBrowser_ = FileBrowser(
            "Resources/Textures/",
            { ".png", ".jpg", ".jpeg", ".dds" },
            FileBrowser::DisplayMode::Grid);

        rampBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
            TextureManager::GetInstance()->LoadTexture(path);
            auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
            return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
            });

        rampBrowser_.SetOnFileSelected([this](const std::string& path) {
            auto* sel = Selected();
            if (!sel) return;
            VfxEffectAsset before = sel->asset;
            sel->asset.trail.texturePath = path;
            CommitChange(before, "Trail ランプテクスチャ");
            showRampPopup_ = false;
            });

        // ---- ノイズテクスチャ用 (t0: gTexNoise) ----
        noiseBrowser_ = FileBrowser(
            "Resources/Textures/",
            { ".png", ".jpg", ".jpeg", ".dds" },
            FileBrowser::DisplayMode::Grid);

        noiseBrowser_.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
            TextureManager::GetInstance()->LoadTexture(path);
            auto handle = TextureManager::GetInstance()->GetsrvHandleGPU(path);
            return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
            });

        noiseBrowser_.SetOnFileSelected([this](const std::string& path) {
            auto* sel = Selected();
            if (!sel) return;
            VfxEffectAsset before = sel->asset;
            sel->asset.trail.noiseTexturePath = path;
            CommitChange(before, "Trail ノイズテクスチャ");
            showNoisePopup_ = false;
            });
    }

    VfxMeshEditor* VfxMeshEditor::GetInstance()
    {
        static VfxMeshEditor instance;
        return &instance;
    }

    void VfxMeshEditor::Initialize(const std::string& scanRoot)
    {
        dxCommon_ = DirectXCommon::GetInstance();
        scanRoot_ = scanRoot;

        // ★ Trail用のプレビューはEmitterに委譲
        previewTrailEmitter_ = std::make_unique<TrailMeshEmitter>();
        previewVolume_ = std::make_unique<LightVolumeMesh>();

        InitCBVs();
        ScanDirectory(scanRoot_);

        if (!entries_.empty()) {
            SelectEffect(0);
        }
    }

    void VfxMeshEditor::Finalize()
    {
        if (volumeCBMapped_ && volumeCBResource_) {
            volumeCBResource_->Unmap(0, nullptr);
            volumeCBMapped_ = nullptr;
        }
        entries_.clear();
        selectedIndex_ = -1;
    }

    void VfxMeshEditor::ScanDirectory(const std::string& dir)
    {
        entries_.clear();

        std::error_code ec;
        if (!fs::exists(dir, ec)) {
            Logger("VfxMeshEditor: スキャン対象ディレクトリが存在しません: " + dir);
            return;
        }

        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.path().extension() != ".json") continue;

            VfxEffectEntry e;
            e.filePath = entry.path().string();
            if (e.asset.LoadFromJson(e.filePath)) {
                entries_.push_back(std::move(e));
                Logger("VfxMeshEditor: ロード -> " + entries_.back().filePath);
            }
        }
        Logger("VfxMeshEditor: " + std::to_string(entries_.size()) + " エフェクトをロードしました");
    }

    void VfxMeshEditor::SelectEffect(int index)
    {
        if (index < 0 || index >= static_cast<int>(entries_.size())) return;

        selectedIndex_ = index;
        history_.Clear();

        previewTimer_ = 0.f;
        previewPlaying_ = false;

        // ★ Emitter に現在のアセットを反映させる
        if (previewTrailEmitter_) {
            previewTrailEmitter_->Stop();
            previewTrailEmitter_->SetCamera(camera_);
            previewTrailEmitter_->LoadAsset(entries_[selectedIndex_].filePath);
            previewTrailEmitter_->SetAsset(entries_[selectedIndex_].asset); // 最新状態を同期
        }

        previewTrailEmitter_->Play();

        RebuildPreviewMeshes();
    }

    void VfxMeshEditor::Update(float deltaTime)
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        previewTimer_ += deltaTime;
        const auto& asset = sel->asset;

        // ★ Emitterを使って頂点を更新
        if (asset.useTrail && previewTrailEmitter_) {
            Vector3 tip = previewCenter_;
            Vector3 root = previewCenter_;
            Vector3 widthDir = { 0.f, 1.f, 0.f };

            const float period = std::max(asset.trail.lifetime * 1.5f, 0.5f);
            const float t = std::fmod(previewTimer_, period) / period;

            switch (previewAnim_) {
            case PreviewAnimMode::Wobble: {
                const float swing = std::sin(t * 3.14159f * 2.f);
                tip = { previewCenter_.x + swing * 0.5f, previewCenter_.y + swordLength_ * 0.5f, previewCenter_.z };
                root = { previewCenter_.x + swing * 0.5f, previewCenter_.y - swordLength_ * 0.5f, previewCenter_.z };
                widthDir = { 1.f, 0.f, 0.f };
                break;
            }
            case PreviewAnimMode::SlashHorizontal: {
                const float easeT = std::pow(t, 0.3f);
                const float angle = -3.14159f + easeT * 3.14159f * 1.5f;
                tip = { previewCenter_.x + std::cos(angle) * swordLength_, previewCenter_.y, previewCenter_.z + std::sin(angle) * swordLength_ };
                root = previewCenter_;
                widthDir = { 0.f, 1.f, 0.f };
                break;
            }
            case PreviewAnimMode::SlashVertical: {
                const float easeT = std::pow(t, 0.3f);
                const float angle = 3.14159f * 0.8f - easeT * 3.14159f * 1.6f;
                tip = { previewCenter_.x, previewCenter_.y + std::sin(angle) * swordLength_, previewCenter_.z + std::cos(angle) * swordLength_ };
                root = previewCenter_;
                widthDir = { 1.f, 0.f, 0.f };
                break;
            }
            case PreviewAnimMode::Spin: {
                const float angle = t * 3.14159f * 2.f;
                tip = { previewCenter_.x + std::cos(angle) * swordLength_, previewCenter_.y, previewCenter_.z + std::sin(angle) * swordLength_ };
                root = previewCenter_;
                widthDir = { 0.f, 1.f, 0.f };
                break;
            }
            }

            // ★ Emitterに点を追加（widthDirも渡す）
            previewTrailEmitter_->AddPoint(tip, root, widthDir);
            previewTrailEmitter_->Update(deltaTime);
        }

        if (asset.useLightVolume) {
            previewVolume_->ApplyParam(asset.lightVolume);
            previewVolume_->SetTransform(previewCenter_, previewYaw_);
            previewVolume_->Update(deltaTime);
        }
    }

    // ===========================================================
    // ★ 描画（cmdListを受け取らず、Emitter側のDrawを呼ぶ）
    // ===========================================================
    void VfxMeshEditor::DrawPreview()
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        const auto& asset = sel->asset;

        // ★ Trail は Emitter 内で完結して描画される
        if (asset.useTrail && previewTrailEmitter_) {
            previewTrailEmitter_->SetCamera(camera_);
            previewTrailEmitter_->Draw();
        }

        // Volume は従来通り
        if (asset.useLightVolume && camera_) {
            auto* pm = YPipelineManager::GetInstance();
            const auto& idx = pm->GetParameterIndices("VfxMeshVolume");
            UpdateVolumeCBV(previewTimer_);

            auto* cmdList = DirectXCommon::GetInstance()->GetCommandList().Get();
            cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshVolume"));
            cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshVolume"));
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camera_->GetCameraResource()->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), volumeCBResource_->GetGPUVirtualAddress());

            previewVolume_->Draw(cmdList);
        }
    }

    void VfxMeshEditor::DrawImGui()
    {
        history_.HandleKeyInput();

        ImGui::SetNextWindowSize(ImVec2(760, 680), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("VFX Mesh Editor")) { ImGui::End(); return; }

        ImGui::BeginChild("##list", ImVec2(200, 0), true);
        DrawListPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##edit", ImVec2(0, 0), true);
        DrawEditPanel();
        ImGui::EndChild();

        DrawNewEffectDialog();

        ImGui::End();
    }

    void VfxMeshEditor::DrawListPanel()
    {
        ImGui::TextDisabled("エフェクト一覧");
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) { showNewDialog_ = true; }
        ImGui::SameLine();
        if (ImGui::SmallButton("R")) {
            int prevSel = selectedIndex_;
            ScanDirectory(scanRoot_);
            selectedIndex_ = -1;
            if (prevSel >= 0 && prevSel < static_cast<int>(entries_.size())) {
                SelectEffect(prevSel);
            }
            else if (!entries_.empty()) {
                SelectEffect(0);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("ディレクトリを再スキャン");

        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            const auto& e = entries_[i];

            std::string label = e.asset.name;
            if (e.isDirty) label += " *";

            bool selected = (selectedIndex_ == i);
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                SelectEffect(i);
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("保存")) {
                    selectedIndex_ = i;
                    SaveCurrent();
                }
                if (ImGui::MenuItem("削除")) {
                    selectedIndex_ = i;
                    DeleteCurrent();
                    ImGui::EndPopup();
                    break;
                }
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", e.filePath.c_str());
            }
        }

        if (entries_.empty()) {
            ImGui::TextDisabled("(エフェクトなし)");
        }
    }

    void VfxMeshEditor::DrawEditPanel()
    {
        auto* sel = Selected();
        if (!sel) {
            ImGui::TextDisabled("エフェクトを選択してください");
            return;
        }
        auto& asset = sel->asset;

        history_.DrawImGui();
        ImGui::SameLine();
        if (ImGui::Button("保存")) SaveCurrent();
        if (sel->isDirty) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.1f, 1.f), "* 未保存");
        }
        ImGui::Separator();

        ImGui::TextDisabled("Path: %s", sel->filePath.c_str());
        ImGui::Separator();

        strncpy_s(nameBuffer_, asset.name.c_str(), sizeof(nameBuffer_));
        ImGui::SetNextItemWidth(-1);
        VfxEffectAsset before = asset;
        if (ImGui::InputText("##effectname", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            asset.name = nameBuffer_;
            CommitChange(before, "名前変更");
        }
        ImGui::SameLine(0, 4); ImGui::TextDisabled("名前");
        ImGui::Separator();

        {
            VfxEffectAsset b = asset;
            ImGui::PushID("UseTrail");
            bool changed = ImGui::Checkbox("##UseTrail", &asset.useTrail);
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNodeEx("Trail", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (asset.useTrail) DrawTrailSection();
                else                ImGui::TextDisabled("  (disabled)");
                ImGui::TreePop();
            }
            if (changed) CommitChange(b, "Trail 有効切替");
        }
        ImGui::Separator();

        {
            VfxEffectAsset b = asset;
            ImGui::PushID("UseLightVolume");
            bool changed = ImGui::Checkbox("##UseLightVolume", &asset.useLightVolume);
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNodeEx("Light Volume", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (asset.useLightVolume) DrawLightVolumeSection();
                else                      ImGui::TextDisabled("  (disabled)");
                ImGui::TreePop();
            }
            if (changed) CommitChange(b, "Volume 有効切替");
        }
        ImGui::Separator();

        DrawPreviewSection();
    }

    void VfxMeshEditor::DrawTrailSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& t = sel->asset.trail;

        ImGui::SeparatorText("形状設定");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;

            const char* shapeNames[] = { "Flat (平板)", "Arc (円弧)", "Fan (扇形)" ,"Custom (カスタム)" };
            int shapeIdx = static_cast<int>(t.shapeType);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##shape", &shapeIdx, shapeNames, IM_ARRAYSIZE(shapeNames))) {
                t.shapeType = static_cast<TrailShapeType>(shapeIdx);
                c = true;
            }
            ImGui::SameLine(0, 4); ImGui::TextDisabled("断面形状");

            if (t.shapeType != TrailShapeType::Flat) {
                c |= ImGui::SliderInt("幅の分割数##wseg", &t.widthSegments, 1, 16);
                c |= ImGui::SliderFloat("円弧の角度(度)##arcang", &t.arcAngleDeg, 10.f, 360.f, "%.1f");
            }
            c |= ImGui::DragInt("滑らかさ(補間分割数)##spline", &t.splineSubdivisions, 1, 512);

            if (c) CommitChange(b, "Trail 形状設定");
        }

        ImGui::SeparatorText("立体感・三日月化");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::Checkbox("三日月カーブにする##crescent", &t.crescentShape);
            c |= ImGui::SliderFloat("厚み (Thickness)##thick", &t.thickness, 0.0f, 2.0f, "%.3f");
            if (c) CommitChange(b, "Trail 形状モディファイア");
        }

        ImGui::SeparatorText("カスタムメッシュ形状エディター");
        if (sel) {
            VfxEffectAsset b = sel->asset;
            bool changed = false;

            if (ImGui::Button("頂点をすべてクリア")) {
                t.customVertices.clear();
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("左クリック: 追加/移動 | 右クリック: 削除");

            ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
            ImVec2 canvas_sz = ImVec2(ImGui::GetContentRegionAvail().x, 300.0f);
            ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 30, 30, 255));
            draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(100, 100, 100, 255));

            ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool is_hovered = ImGui::IsItemHovered();
            const bool is_active = ImGui::IsItemActive();
            const ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_p0.x, ImGui::GetIO().MousePos.y - canvas_p0.y);

            ImVec2 origin(canvas_p0.x + canvas_sz.x * 0.5f, canvas_p0.y + canvas_sz.y * 0.5f);
            const float zoom = 50.0f;
            static int dragging_idx = -1;

            if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                dragging_idx = -1;
                for (int i = 0; i < t.customVertices.size(); ++i) {
                    ImVec2 p(origin.x + t.customVertices[i].x * zoom, origin.y - t.customVertices[i].y * zoom);
                    float dx = ImGui::GetIO().MousePos.x - p.x;
                    float dy = ImGui::GetIO().MousePos.y - p.y;
                    if (dx * dx + dy * dy < 100.0f) {
                        dragging_idx = i;
                        break;
                    }
                }
                if (dragging_idx == -1) {
                    t.customVertices.push_back({ (mouse_pos_in_canvas.x - canvas_sz.x * 0.5f) / zoom,
                                                -(mouse_pos_in_canvas.y - canvas_sz.y * 0.5f) / zoom });
                    dragging_idx = static_cast<int>(t.customVertices.size()) - 1;
                    changed = true;
                }
            }

            if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && dragging_idx >= 0) {
                t.customVertices[dragging_idx].x += ImGui::GetIO().MouseDelta.x / zoom;
                t.customVertices[dragging_idx].y -= ImGui::GetIO().MouseDelta.y / zoom;
                changed = true;
            }
            else if (!is_active) {
                dragging_idx = -1;
            }

            if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                for (int i = 0; i < t.customVertices.size(); ++i) {
                    ImVec2 p(origin.x + t.customVertices[i].x * zoom, origin.y - t.customVertices[i].y * zoom);
                    float dx = ImGui::GetIO().MousePos.x - p.x;
                    float dy = ImGui::GetIO().MousePos.y - p.y;
                    if (dx * dx + dy * dy < 100.0f) {
                        t.customVertices.erase(t.customVertices.begin() + i);
                        changed = true;
                        break;
                    }
                }
            }

            draw_list->AddLine(ImVec2(origin.x, canvas_p0.y), ImVec2(origin.x, canvas_p1.y), IM_COL32(100, 100, 100, 150));
            draw_list->AddLine(ImVec2(canvas_p0.x, origin.y), ImVec2(canvas_p1.x, origin.y), IM_COL32(100, 100, 100, 150));

            if (t.customVertices.size() >= 2) {
                for (int i = 0; i < t.customVertices.size(); ++i) {
                    int next_i = (i + 1) % t.customVertices.size();
                    ImVec2 p1(origin.x + t.customVertices[i].x * zoom, origin.y - t.customVertices[i].y * zoom);
                    ImVec2 p2(origin.x + t.customVertices[next_i].x * zoom, origin.y - t.customVertices[next_i].y * zoom);
                    draw_list->AddLine(p1, p2, IM_COL32(0, 255, 255, 255), 2.0f);
                }
            }

            for (int i = 0; i < t.customVertices.size(); ++i) {
                ImVec2 p(origin.x + t.customVertices[i].x * zoom, origin.y - t.customVertices[i].y * zoom);
                draw_list->AddCircleFilled(p, 5.0f, (i == dragging_idx) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255));
            }

            if (changed) CommitChange(b, "カスタムメッシュ編集");
        }

        ImGui::SeparatorText("幅");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("根元幅##ws", &t.widthStart, 0.f, 5.f, "%.3f");
            c |= ImGui::SliderFloat("先端幅##we", &t.widthEnd, 0.f, 5.f, "%.3f");
            if (c) CommitChange(b, "Trail 幅");
        }

        ImGui::SeparatorText("寿命 / ポイント数");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("寿命 (秒)##lt", &t.lifetime, 0.05f, 5.f, "%.2f");
            c |= ImGui::SliderInt("最大ポイント##mp", &t.maxPoints, 4, 512);
            if (c) CommitChange(b, "Trail 寿命");
        }

        ImGui::SeparatorText("カラーグラデーション");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::ColorEdit4("根元カラー##cs", &t.colorStart.x);
            c |= ImGui::ColorEdit4("先端カラー##ce", &t.colorEnd.x);
            if (c) CommitChange(b, "Trail カラー");
        }

        ImGui::SeparatorText("ブレンドモード");
        {
            VfxEffectAsset b = sel->asset;
            const char* names[] = { "通常", "加算", "減算", "乗算" };
            int idx = static_cast<int>(t.blendMode);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##blend", &idx, names, IM_ARRAYSIZE(names))) {
                t.blendMode = static_cast<BlendMode>(idx);
                CommitChange(b, "Trail ブレンド");
            }
        }

        ImGui::SeparatorText("シェーダー");
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::SliderFloat("UV スクロール速度##uvs", &t.uvScrollSpeed, -5.f, 5.f, "%.2f"))
                CommitChange(b, "Trail UV スクロール");
        }

        ImGui::SeparatorText("テクスチャ");
        {
            ImGui::TextDisabled("ランプ (t1)");
            ImGui::SameLine(80);
            ImGui::TextDisabled("%s", t.texturePath.empty() ? "(未設定)" : t.texturePath.c_str());

            if (ImGui::SmallButton("参照##ramp")) {
                rampBrowser_.Scan();
                showRampPopup_ = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("クリア##ramp")) {
                VfxEffectAsset b = sel->asset;
                t.texturePath.clear();
                CommitChange(b, "Trail ランプテクスチャクリア");
            }

            ImGui::Spacing();

            ImGui::TextDisabled("ノイズ (t0)");
            ImGui::SameLine(80);
            ImGui::TextDisabled("%s", t.noiseTexturePath.empty() ? "(未設定)" : t.noiseTexturePath.c_str());

            if (ImGui::SmallButton("参照##noise")) {
                noiseBrowser_.Scan();
                showNoisePopup_ = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("クリア##noise")) {
                VfxEffectAsset b = sel->asset;
                t.noiseTexturePath.clear();
                CommitChange(b, "Trail ノイズテクスチャクリア");
            }

            DrawTextureSelectPopup();
        }

        ImGui::Spacing();
        ImGui::Checkbox("Trail デバッグ表示", &showTrailDebug_);
        if (showTrailDebug_) ImGui::TextColored(ImVec4(1, 1, 0, 1), "  > デバッグオーバーレイ ON");
    }

    void VfxMeshEditor::DrawTextureSelectPopup()
    {
        if (showRampPopup_) ImGui::OpenPopup("##RampTexSelect");

        ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("##RampTexSelect", &showRampPopup_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
        {
            ImGui::Text("ランプテクスチャを選択 (t1: gTexRamp)");
            ImGui::Separator();
            rampBrowser_.Draw("##RampBrowserChild", ImVec2(0, 340));
            ImGui::Separator();
            if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
                showRampPopup_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (showNoisePopup_) ImGui::OpenPopup("##NoiseTexSelect");

        ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("##NoiseTexSelect", &showNoisePopup_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
        {
            ImGui::Text("ノイズテクスチャを選択 (t0: gTexNoise)");
            ImGui::Separator();
            noiseBrowser_.Draw("##NoiseBrowserChild", ImVec2(0, 340));
            ImGui::Separator();
            if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
                showNoisePopup_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void VfxMeshEditor::DrawLightVolumeSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& lv = sel->asset.lightVolume;

        ImGui::SeparatorText("OBB サイズ  (X=右 / Y=上 / Z=前)");
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::DragFloat3("半辺長##he", &lv.halfExtents.x, 0.05f, 0.01f, 50.f, "%.2f"))
                CommitChange(b, "Volume サイズ");
        }

        ImGui::SeparatorText("カラー  (α = ボリューム濃度)");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::ColorEdit4("ボリュームカラー##vc", &lv.color.x);
            c |= ImGui::SliderFloat("輝度##inten", &lv.intensity, 0.f, 10.f, "%.2f");
            if (c) CommitChange(b, "Volume カラー");
        }

        ImGui::Spacing();
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::Checkbox("ボリューム有効##en", &lv.isEnable))
                CommitChange(b, "Volume 有効切替");
        }

        ImGui::Spacing();
        ImGui::Checkbox("OBB ワイヤーフレーム表示", &showVolumeDebug_);
        if (showVolumeDebug_) ImGui::TextColored(ImVec4(1, 1, 0, 1), "  > OBB ワイヤーフレーム ON");
    }

    void VfxMeshEditor::DrawPreviewSection()
    {
        ImGui::SeparatorText("プレビュー");
        const char* animNames[] = { "Wobble (往復)", "Slash (横なぎ)", "Slash (縦斬り)", "Spin (回転)" };
        int animIdx = static_cast<int>(previewAnim_);
        ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("軌道アニメ", &animIdx, animNames, IM_ARRAYSIZE(animNames))) {
            previewAnim_ = static_cast<PreviewAnimMode>(animIdx);
            if (previewTrailEmitter_) previewTrailEmitter_->Play(); // アニメ変更時にリセットして再生
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("剣の長さ", &swordLength_, 0.1f, 0.5f, 10.f, "%.1f");

        if (previewPlaying_) {
            // ★ 停止ボタン
            if (ImGui::Button((std::string(Icon::Stop) + "停止").c_str())) {
                previewPlaying_ = false;
                if (previewTrailEmitter_) previewTrailEmitter_->Stop(); // Emitterを停止（描画されなくなる）
            }
        }
        else {
            // ★ 再生ボタン
            if (ImGui::Button((std::string(Icon::Play) + "再生").c_str())) {
                previewPlaying_ = true;
                previewTimer_ = 0.f;
                if (previewTrailEmitter_) previewTrailEmitter_->Play(); // Emitterを再生（描画再開）
            }
        }
        ImGui::SameLine();
        // ★ リセットボタン
        if (ImGui::Button((std::string(Icon::Refresh) + "リセット").c_str())) {
            previewTimer_ = 0.f;
            if (previewTrailEmitter_) previewTrailEmitter_->Play(); // リセットして最初から再生
        }

        ImGui::DragFloat3("プレビュー位置", &previewCenter_.x, 0.05f, -20.f, 20.f);
        ImGui::SliderAngle("プレビューYaw", &previewYaw_, -180.f, 180.f);

        auto* sel = Selected();
        if (sel) {
            if (sel->asset.useTrail && sel->asset.trail.lifetime > 0.f) {
                char ov[48];
                snprintf(ov, sizeof(ov), "Trail  %.2f / %.2f s",
                    previewTimer_, sel->asset.trail.lifetime);
                ImGui::ProgressBar(
                    std::min(previewTimer_ / sel->asset.trail.lifetime, 1.f),
                    ImVec2(-1, 0), ov);
            }
            if (sel->asset.useLightVolume && sel->asset.lightVolume.isEnable) {
                ImGui::ProgressBar(1.f, ImVec2(-1, 0), "Light Volume  (active)");
            }
        }
        ImGui::TextDisabled("時刻: %.3f s", previewTimer_);
    }
    void VfxMeshEditor::DrawNewEffectDialog()
    {
        if (!showNewDialog_) return;

        ImGui::OpenPopup("新規エフェクト作成");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal("新規エフェクト作成", &showNewDialog_, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("エフェクト名");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newname", newNameBuffer_, sizeof(newNameBuffer_));

        ImGui::Spacing();
        ImGui::Text("保存先 JSON パス");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newpath", newPathBuffer_, sizeof(newPathBuffer_));

        if (newPathBuffer_[0] == '\0' && newNameBuffer_[0] != '\0') {
            std::string autoPath = scanRoot_ + newNameBuffer_ + ".json";
            ImGui::TextDisabled("→ %s", autoPath.c_str());
        }

        ImGui::Spacing();
        ImGui::Text("プリセット");
        const char* presetNames[] = { "Blank", "Trail Only", "Volume Only", "Sword", "Magic" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##preset", &newPresetIdx_, presetNames, IM_ARRAYSIZE(presetNames));

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Button("作成", ImVec2(120, 0))) {
            std::string path = newPathBuffer_;
            if (path.empty()) path = scanRoot_ + newNameBuffer_ + ".json";

            CreateNew(newNameBuffer_, path, static_cast<VfxPreset>(newPresetIdx_));
            showNewDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
            showNewDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void VfxMeshEditor::SaveCurrent()
    {
        auto* sel = Selected();
        if (!sel) return;

        fs::path p(sel->filePath);
        if (!p.parent_path().empty()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
        sel->asset.SaveToJson(sel->filePath);
        sel->isDirty = false;
        Logger("VfxMeshEditor: 保存 -> " + sel->filePath);
    }

    void VfxMeshEditor::SaveAs(const std::string& newPath)
    {
        auto* sel = Selected();
        if (!sel) return;

        sel->filePath = newPath;
        SaveCurrent();
    }

    void VfxMeshEditor::DeleteCurrent()
    {
        auto* sel = Selected();
        if (!sel) return;

        std::error_code ec;
        fs::remove(sel->filePath, ec);
        if (ec) Logger("VfxMeshEditor: 削除失敗 -> " + sel->filePath);
        else    Logger("VfxMeshEditor: 削除 -> " + sel->filePath);

        entries_.erase(entries_.begin() + selectedIndex_);

        if (entries_.empty()) {
            selectedIndex_ = -1;
        }
        else {
            selectedIndex_ = std::min(selectedIndex_, static_cast<int>(entries_.size()) - 1);
            SelectEffect(selectedIndex_);
        }
    }

    void VfxMeshEditor::CreateNew(const std::string& name,
        const std::string& filePath,
        VfxPreset          preset)
    {
        VfxEffectEntry e;
        e.asset = MakePreset(preset);
        e.asset.name = name;
        e.filePath = filePath;
        e.isDirty = true;
        entries_.push_back(std::move(e));

        SelectEffect(static_cast<int>(entries_.size()) - 1);
        SaveCurrent();

        Logger("VfxMeshEditor: 新規作成 -> " + filePath);
    }

    // ★ 編集を即時プレビューに反映
    void VfxMeshEditor::CommitChange(const VfxEffectAsset& before, const char* label)
    {
        auto* sel = Selected();
        if (!sel) return;

        VfxEffectAsset after = sel->asset;
        const int idx = selectedIndex_;

        history_.Execute(MakeLambdaCommand(
            label,
            [this, idx, after]() {
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = after;
                    entries_[idx].isDirty = true;
                    if (previewTrailEmitter_) previewTrailEmitter_->SetAsset(after); // ★即時反映
                    if (previewVolume_) previewVolume_->ApplyParam(after.lightVolume);
                }
            },
            [this, idx, before]() {
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = before;
                    entries_[idx].isDirty = true;
                    if (previewTrailEmitter_) previewTrailEmitter_->SetAsset(before); // ★Undo即時反映
                    if (previewVolume_) previewVolume_->ApplyParam(before.lightVolume);
                }
            }
        ));

        sel->isDirty = true;

        // 今回の編集分もプレビューに反映させる
        if (previewTrailEmitter_) {
            previewTrailEmitter_->SetAsset(sel->asset);
        }
    }

    void VfxMeshEditor::RebuildPreviewMeshes()
    {
        auto* sel = Selected();
        if (!sel) return;
        previewVolume_->Initialize(sel->asset.lightVolume);
    }

    void VfxMeshEditor::InitCBVs()
    {
        volumeCBResource_ = dxCommon_->CreateBufferResource(AlignedSize<LightVolumeParamsCB>());
        volumeCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&volumeCBMapped_));
    }

    void VfxMeshEditor::UpdateVolumeCBV(float time)
    {
        auto* sel = Selected();
        if (!sel || !volumeCBMapped_) return;
        const auto& lv = sel->asset.lightVolume;

        volumeCBMapped_->color[0] = lv.color.x;
        volumeCBMapped_->color[1] = lv.color.y;
        volumeCBMapped_->color[2] = lv.color.z;
        volumeCBMapped_->color[3] = lv.color.w * lv.intensity;
        volumeCBMapped_->edgeFade = 0.15f;
        volumeCBMapped_->depthFade = 1.0f;
        volumeCBMapped_->noiseTiling = 2.0f;
        volumeCBMapped_->noiseStrength = 0.0f;
        volumeCBMapped_->time = time;
    }

    VfxEffectAsset VfxMeshEditor::MakePreset(VfxPreset preset)
    {
        VfxEffectAsset a;
        switch (preset) {
        case VfxPreset::TrailOnly:
            a.useTrail = true; a.useLightVolume = false; break;
        case VfxPreset::VolumeOnly:
            a.useTrail = false; a.useLightVolume = true; break;
        case VfxPreset::Sword:
            a.useTrail = true; a.useLightVolume = true;
            a.trail.widthStart = 0.05f; a.trail.widthEnd = 0.f;
            a.trail.lifetime = 0.25f; a.trail.maxPoints = 48;
            a.trail.colorStart = { 1.f, 0.95f, 0.7f, 1.f };
            a.trail.colorEnd = { 0.8f, 0.5f, 0.1f, 0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.3f;
            a.lightVolume.halfExtents = { 0.04f, 0.04f, 0.6f };
            a.lightVolume.color = { 1.f, 0.9f, 0.5f, 0.18f };
            a.lightVolume.intensity = 1.5f;
            break;
        case VfxPreset::Magic:
            a.useTrail = true; a.useLightVolume = true;
            a.trail.widthStart = 0.4f;  a.trail.widthEnd = 0.05f;
            a.trail.lifetime = 1.2f;  a.trail.maxPoints = 96;
            a.trail.colorStart = { 0.4f, 0.2f, 1.f, 1.f };
            a.trail.colorEnd = { 0.2f, 0.6f, 1.f, 0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.8f;
            a.lightVolume.halfExtents = { 1.5f, 1.5f, 4.f };
            a.lightVolume.color = { 0.5f, 0.3f, 1.f, 0.25f };
            a.lightVolume.intensity = 3.f;
            break;
        default: break;
        }
        return a;
    }

} // namespace YoRigine
#endif