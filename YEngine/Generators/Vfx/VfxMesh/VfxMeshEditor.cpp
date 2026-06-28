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
        previewSmoke_ = std::make_unique<VolumeSmokeMesh>();
        previewSmoke_->Initialize();
        previewLightning_ = std::make_unique<LightningMesh>();
        previewLightning_->Initialize();

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
        if (smokeCBMapped_ && smokeCBResource_) {
            smokeCBResource_->Unmap(0, nullptr);
            smokeCBMapped_ = nullptr;
        }
        if (lightningCBMapped_ && lightningCBResource_) {
            lightningCBResource_->Unmap(0, nullptr);
            lightningCBMapped_ = nullptr;
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

        if (asset.useSmoke && previewSmoke_) {
            previewSmoke_->SetTransform(previewCenter_, asset.smoke.radius);
            previewSmoke_->Update(deltaTime);
        }

        if (asset.useLightning && previewLightning_) {
            previewLightning_->SetCamera(camera_);
            previewLightning_->ApplyParam(asset.lightning);
            // プレビュー: 中心を挟んで上下に length の長さで伸ばす
            float half = asset.lightning.length * 0.5f;
            previewLightning_->SetEndpoints(previewCenter_ + Vector3{ 0, -half, 0 },
                                            previewCenter_ + Vector3{ 0,  half, 0 });
            previewLightning_->Update(deltaTime);
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

        // Volume Smoke (Omen 風)
        if (asset.useSmoke && asset.smoke.isEnable && camera_ && previewSmoke_) {
            auto* pm = YPipelineManager::GetInstance();
            const auto& idx = pm->GetParameterIndices("VfxMeshSmoke");
            UpdateSmokeCBV(previewTimer_);

            auto* cmdList = DirectXCommon::GetInstance()->GetCommandList().Get();
            cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshSmoke"));
            cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshSmoke"));
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camera_->GetCameraResource()->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), smokeCBResource_->GetGPUVirtualAddress());

            previewSmoke_->Draw(cmdList);
        }

        // Lightning (プロシージャル稲妻)
        if (asset.useLightning && asset.lightning.isEnable && camera_ && previewLightning_) {
            auto* pm = YPipelineManager::GetInstance();
            const auto& idx = pm->GetParameterIndices("VfxMeshLightning");
            UpdateLightningCBV(previewTimer_);

            auto* cmdList = DirectXCommon::GetInstance()->GetCommandList().Get();
            cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshLightning"));
            cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshLightning"));
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camera_->GetCameraResource()->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), lightningCBResource_->GetGPUVirtualAddress());

            previewLightning_->Draw(cmdList);
        }
    }

    void VfxMeshEditor::DrawImGui()
    {
        history_.HandleKeyInput();

        ImGui::SetNextWindowSize(ImVec2(760, 680), ImGuiCond_FirstUseEver);

        ImGui::BeginChild("##list", ImVec2(200, 0), true);
        DrawListPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##edit", ImVec2(0, 0), true);
        DrawEditPanel();
        ImGui::EndChild();

        DrawNewEffectDialog();
    }

    void VfxMeshEditor::DrawListPanel()
    {
        ImGui::TextDisabled("エフェクト一覧");
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) {
            // 既存名と衝突しない初期名をセット（NewEffect / NewEffect1 / NewEffect2 ...）
            std::string uniq = MakeUniqueEffectName("NewEffect");
            strncpy_s(newNameBuffer_, sizeof(newNameBuffer_), uniq.c_str(), _TRUNCATE);
            newPathBuffer_[0] = '\0';
            showNewDialog_ = true;
        }
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

        {
            VfxEffectAsset b = asset;
            ImGui::PushID("UseSmoke");
            bool changed = ImGui::Checkbox("##UseSmoke", &asset.useSmoke);
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNodeEx("Volume Smoke (Omen風)", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (asset.useSmoke) DrawSmokeSection();
                else                ImGui::TextDisabled("  (disabled)");
                ImGui::TreePop();
            }
            if (changed) CommitChange(b, "Smoke 有効切替");
        }
        ImGui::Separator();

        {
            VfxEffectAsset b = asset;
            ImGui::PushID("UseLightning");
            bool changed = ImGui::Checkbox("##UseLightning", &asset.useLightning);
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNodeEx("Lightning (稲妻)", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (asset.useLightning) DrawLightningSection();
                else                    ImGui::TextDisabled("  (disabled)");
                ImGui::TreePop();
            }
            if (changed) CommitChange(b, "Lightning 有効切替");
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

            const char* shapeNames[] = {
                "Flat (平板)", "Arc (円弧)", "Fan (扇形)",
                "Custom (2D ポリゴン押し出し)", "Primitive (3D メッシュ)"
            };
            int shapeIdx = static_cast<int>(t.shapeType);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##shape", &shapeIdx, shapeNames, IM_ARRAYSIZE(shapeNames))) {
                t.shapeType = static_cast<TrailShapeType>(shapeIdx);
                c = true;
            }
            ImGui::SameLine(0, 4); ImGui::TextDisabled("断面形状");

            // Arc / Fan のみ widthSegments + 円弧角を出す (Flat/Custom/Primitive は不要)
            if (t.shapeType == TrailShapeType::Arc || t.shapeType == TrailShapeType::Fan) {
                c |= ImGui::SliderInt("幅の分割数##wseg", &t.widthSegments, 1, 16);
                c |= ImGui::SliderFloat("円弧の角度(度)##arcang", &t.arcAngleDeg, 10.f, 360.f, "%.1f");
            }
            c |= ImGui::DragInt("滑らかさ(補間分割数)##spline", &t.splineSubdivisions, 1, 512);

            if (c) CommitChange(b, "Trail 形状設定");
        }

        // ★ Primitive (3D) 専用セクション
        if (t.shapeType == TrailShapeType::Primitive) {
            ImGui::SeparatorText("3D プリミティブ");
            VfxEffectAsset b = sel->asset;
            bool c = false;
            auto& sp = t.primitive;

            // Type
            const char* typeNames[] = { "Box", "Sphere", "Capsule", "Cone", "Cylinder", "Torus" };
            int typeIdx = static_cast<int>(sp.type);
            ImGui::SetNextItemWidth(180);
            if (ImGui::Combo("Type##prim", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
                sp.type = static_cast<PrimitiveType>(typeIdx);
                c = true;
            }

            // Placement
            const char* placeNames[] = { "Static (1個固定)", "BeadAlongTrail (軌跡に連続配置)" };
            int placeIdx = static_cast<int>(sp.placement);
            ImGui::SetNextItemWidth(260);
            if (ImGui::Combo("配置##prim", &placeIdx, placeNames, IM_ARRAYSIZE(placeNames))) {
                sp.placement = static_cast<PrimitivePlacement>(placeIdx);
                c = true;
            }

            // 共通スケール
            c |= ImGui::DragFloat("スタンプスケール##prim", &sp.stampScale,
                                  0.01f, 0.001f, 100.f, "%.3f");

            // Type 別パラメータ
            ImGui::Spacing();
            switch (sp.type) {
            case PrimitiveType::Box:
                c |= ImGui::DragFloat3("半辺長##bx", &sp.halfExtents.x, 0.01f, 0.001f, 10.f, "%.3f");
                break;
            case PrimitiveType::Sphere:
                c |= ImGui::DragFloat("半径##sp", &sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= ImGui::SliderInt("緯度分割##sp", &sp.latSegments, 2, 64);
                c |= ImGui::SliderInt("経度分割##sp", &sp.lonSegments, 3, 64);
                break;
            case PrimitiveType::Capsule:
                c |= ImGui::DragFloat("半径##cap", &sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= ImGui::DragFloat("全高##cap", &sp.height, 0.01f, 0.001f, 20.f, "%.3f");
                c |= ImGui::SliderInt("緯度分割##cap", &sp.latSegments, 2, 32);
                c |= ImGui::SliderInt("経度分割##cap", &sp.lonSegments, 3, 32);
                break;
            case PrimitiveType::Cone:
                c |= ImGui::DragFloat("底面半径##cn", &sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= ImGui::DragFloat("高さ##cn", &sp.height, 0.01f, 0.001f, 20.f, "%.3f");
                c |= ImGui::SliderInt("分割##cn", &sp.lonSegments, 3, 64);
                break;
            case PrimitiveType::Cylinder:
                c |= ImGui::DragFloat("半径##cy", &sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= ImGui::DragFloat("高さ##cy", &sp.height, 0.01f, 0.001f, 20.f, "%.3f");
                c |= ImGui::SliderInt("分割##cy", &sp.lonSegments, 3, 64);
                break;
            case PrimitiveType::Torus:
                c |= ImGui::DragFloat("主半径 R##to", &sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= ImGui::DragFloat("管半径 r##to", &sp.tubeRadius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= ImGui::SliderInt("主分割##to", &sp.lonSegments, 3, 64);
                c |= ImGui::SliderInt("管分割##to", &sp.ringSegments, 3, 32);
                break;
            }

            // Bead モード固有
            if (sp.placement == PrimitivePlacement::BeadAlongTrail) {
                ImGui::Spacing();
                ImGui::TextDisabled("Bead モード設定");
                c |= ImGui::DragFloat("間引き間隔##bead", &sp.stampSpacing,
                                      0.01f, 0.0f, 10.0f, "%.3f");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("0 = 平滑化後の全点に配置 / >0 = 直線距離で間引き");
                }
                c |= ImGui::Checkbox("寿命でスケール縮小##bead", &sp.scaleByAge);
            }

            if (c) CommitChange(b, "Primitive 設定");
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

    void VfxMeshEditor::DrawSmokeSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& sm = sel->asset.smoke;

        ImGui::SeparatorText("カラー  (rgb>1 で Bloom / a=濃度)");
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::DragFloat4("色##sm", &sm.color.x, 0.02f, 0.0f, 10.0f, "%.2f"))
                CommitChange(b, "Smoke 色");
        }

        ImGui::SeparatorText("形状 / 渦巻き");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat("半径##smr",          &sm.radius,        0.05f, 0.1f, 20.0f, "%.2f");
            c |= ImGui::DragFloat("ノイズスケール##sms", &sm.noiseScale,    0.05f, 0.1f, 16.0f, "%.2f");
            c |= ImGui::SliderFloat("渦巻きの強さ##smn", &sm.noiseStrength, 0.0f, 1.0f);
            c |= ImGui::DragFloat("スクロール速度##smc", &sm.scrollSpeed,   0.01f, 0.0f, 3.0f, "%.2f");
            c |= ImGui::DragFloat("縁の柔らかさ##smf",   &sm.fresnelPower,  0.05f, 0.1f, 8.0f, "%.2f");
            c |= ImGui::DragFloat("密度##smd",           &sm.density,       0.01f, 0.0f, 3.0f, "%.2f");
            c |= ImGui::DragFloat("オクターブ##smo",     &sm.noiseOctaves,  0.1f,  1.0f, 5.0f, "%.1f");
            c |= ImGui::DragFloat("リム発光(フレア)##smrim", &sm.rimIntensity, 0.05f, 0.0f, 10.0f, "%.2f");
            if (c) CommitChange(b, "Smoke パラメータ");
        }

        ImGui::Spacing();
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::Checkbox("スモーク有効##smen", &sm.isEnable))
                CommitChange(b, "Smoke 有効切替");
        }
    }

    void VfxMeshEditor::DrawLightningSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& lt = sel->asset.lightning;

        ImGui::SeparatorText("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::DragFloat4("色##lt", &lt.color.x, 0.02f, 0.0f, 10.0f, "%.2f"))
                CommitChange(b, "Lightning 色");
        }

        ImGui::SeparatorText("形状 / 明滅");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat("長さ##ltlen",      &lt.length,       0.05f, 0.1f, 50.0f, "%.2f");
            c |= ImGui::DragFloat("幅##ltw",          &lt.width,        0.005f, 0.01f, 2.0f, "%.3f");
            c |= ImGui::DragFloat("ジグザグ振れ##ltj", &lt.jitter,       0.02f, 0.0f, 5.0f, "%.2f");
            c |= ImGui::SliderInt("分割数##lts",       &lt.segments,     4, 64);
            c |= ImGui::SliderInt("枝の数##ltb",       &lt.branches,     0, 8);
            c |= ImGui::DragFloat("枝の振れ##ltbj",    &lt.branchJitter, 0.02f, 0.0f, 5.0f, "%.2f");
            c |= ImGui::DragFloat("明滅レート(回/秒)##ltf", &lt.flickerRate, 0.5f, 0.0f, 60.0f, "%.1f");
            c |= ImGui::DragFloat("芯のグロー##ltg",   &lt.glowPower,    0.05f, 0.1f, 8.0f, "%.2f");
            if (c) CommitChange(b, "Lightning パラメータ");
        }

        ImGui::Spacing();
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::Checkbox("稲妻有効##lten", &lt.isEnable))
                CommitChange(b, "Lightning 有効切替");
        }
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
        ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_Appearing);

        // 固定サイズ。AlwaysAutoResize だと入力に応じてウィンドウサイズが変わってしまうため NoResize に。
        if (!ImGui::BeginPopupModal("新規エフェクト作成", &showNewDialog_, ImGuiWindowFlags_NoResize)) return;

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

    std::string VfxMeshEditor::MakeUniqueEffectName(const std::string& base) const
    {
        auto used = [&](const std::string& n) {
            for (const auto& e : entries_) {
                if (e.asset.name == n) return true;
            }
            return false;
        };
        if (!used(base)) return base;
        for (int i = 1; i < 100000; ++i) {
            std::string cand = base + std::to_string(i);
            if (!used(cand)) return cand;
        }
        return base;
    }

    void VfxMeshEditor::CreateNew(const std::string& name,
        const std::string& filePath,
        VfxPreset          preset)
    {
        // 名前が既存と衝突する場合は採番（手入力で被ったケースも吸収）。パスも追従させる。
        std::string uniqueName = MakeUniqueEffectName(name);
        std::string finalPath  = filePath;
        if (uniqueName != name) finalPath = scanRoot_ + uniqueName + ".json";

        VfxEffectEntry e;
        e.asset = MakePreset(preset);
        e.asset.name = uniqueName;
        e.filePath = finalPath;
        e.isDirty = true;
        entries_.push_back(std::move(e));

        SelectEffect(static_cast<int>(entries_.size()) - 1);
        SaveCurrent();

        Logger("VfxMeshEditor: 新規作成 -> " + finalPath);
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

        smokeCBResource_ = dxCommon_->CreateBufferResource(AlignedSize<SmokeParamsCB>());
        smokeCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&smokeCBMapped_));

        lightningCBResource_ = dxCommon_->CreateBufferResource(AlignedSize<LightningParamsCB>());
        lightningCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightningCBMapped_));
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

    void VfxMeshEditor::UpdateSmokeCBV(float time)
    {
        auto* sel = Selected();
        if (!sel || !smokeCBMapped_) return;
        const auto& sm = sel->asset.smoke;

        smokeCBMapped_->color         = sm.color;
        smokeCBMapped_->center        = previewCenter_;
        smokeCBMapped_->radius        = sm.radius;
        smokeCBMapped_->time          = time;
        smokeCBMapped_->noiseScale    = sm.noiseScale;
        smokeCBMapped_->noiseStrength = sm.noiseStrength;
        smokeCBMapped_->scrollSpeed   = sm.scrollSpeed;
        smokeCBMapped_->fresnelPower  = sm.fresnelPower;
        smokeCBMapped_->density       = sm.density;
        smokeCBMapped_->noiseOctaves  = sm.noiseOctaves;
        smokeCBMapped_->rimIntensity  = sm.rimIntensity;
    }

    void VfxMeshEditor::UpdateLightningCBV(float time)
    {
        auto* sel = Selected();
        if (!sel || !lightningCBMapped_) return;
        const auto& lt = sel->asset.lightning;

        lightningCBMapped_->color      = lt.color;
        lightningCBMapped_->time       = time;
        lightningCBMapped_->glowPower  = lt.glowPower;
        lightningCBMapped_->coreWidthN = 0.0f;
        lightningCBMapped_->_pad       = 0.0f;
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