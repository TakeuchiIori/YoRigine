// ===========================================================
// VfxMeshEditor.cpp
// ===========================================================
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

namespace fs = std::filesystem;

static constexpr size_t kCBVAlignment = 256;
template<typename T>
static constexpr size_t AlignedSize() {
    return (sizeof(T) + kCBVAlignment - 1) & ~(kCBVAlignment - 1);
}

namespace YoRigine {

    // ===========================================================
    // コンストラクタ
    // YParticleSystem と同じパターン:
    //   コンストラクタ本体で FileBrowser を構築し
    //   ThumbnailProvider と OnFileSelected を設定する
    // ===========================================================
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

    // ===========================================================
    // シングルトン
    // ===========================================================
    VfxMeshEditor* VfxMeshEditor::GetInstance()
    {
        static VfxMeshEditor instance;
        return &instance;
    }

    // ===========================================================
    // 初期化 / 終了
    // ===========================================================
    void VfxMeshEditor::Initialize(const std::string& scanRoot)
    {
        dxCommon_ = DirectXCommon::GetInstance();
        scanRoot_ = scanRoot;

        previewTrail_ = std::make_unique<TrailMesh>();
        previewVolume_ = std::make_unique<LightVolumeMesh>();
        InitCBVs();

        // Resources/Vfx/ 以下をスキャンしてエフェクト一覧を構築
        ScanDirectory(scanRoot_);

        // エントリがあれば先頭を選択
        if (!entries_.empty()) {
            SelectEffect(0);
        }
    }

    void VfxMeshEditor::Finalize()
    {
        if (trailCBMapped_ && trailCBResource_) { trailCBResource_->Unmap(0, nullptr);  trailCBMapped_ = nullptr; }
        if (volumeCBMapped_ && volumeCBResource_) { volumeCBResource_->Unmap(0, nullptr); volumeCBMapped_ = nullptr; }
        entries_.clear();
        selectedIndex_ = -1;
    }

    // ===========================================================
    // ディレクトリスキャン
    // ===========================================================
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

    // ===========================================================
    // エフェクト選択
    // ===========================================================
    void VfxMeshEditor::SelectEffect(int index)
    {
        if (index < 0 || index >= static_cast<int>(entries_.size())) return;

        selectedIndex_ = index;
        history_.Clear();

        // プレビューメッシュをリセット
        previewTrail_->Clear();
        previewTimer_ = 0.f;
        previewPlaying_ = false;
        RebuildPreviewMeshes();
    }

    // ===========================================================
    // 毎フレーム
    // ===========================================================
    void VfxMeshEditor::Update(float deltaTime)
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        previewTimer_ += deltaTime;
        const auto& asset = sel->asset;

        if (asset.useTrail) {
            previewTrail_->ApplyParam(asset.trail);

            const float period = std::max(asset.trail.lifetime * 2.f, 0.2f);
            const float t = std::fmod(previewTimer_, period) / period;
            const float swing = std::sin(t * 3.14159f * 2.f);

            const Vector3 tip = { previewCenter_.x + swing * 0.5f,
                                    previewCenter_.y + 1.0f,
                                    previewCenter_.z };
            const Vector3 root = { previewCenter_.x + swing * 0.5f,
                                    previewCenter_.y - 1.0f,
                                    previewCenter_.z };
            previewTrail_->AddPoint(tip, root);
            previewTrail_->Update(deltaTime);
        }

        if (asset.useLightVolume) {
            previewVolume_->ApplyParam(asset.lightVolume);
            previewVolume_->SetTransform(previewCenter_, previewYaw_);
            previewVolume_->Update(deltaTime);
        }
    }

    // ===========================================================
    // DrawPreview
    // ===========================================================
    void VfxMeshEditor::DrawPreview(ID3D12GraphicsCommandList* cmdList,
        D3D12_GPU_VIRTUAL_ADDRESS  cameraGPUAddress)
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        const auto& asset = sel->asset;
        auto* pm = YPipelineManager::GetInstance();

        if (asset.useTrail) {
            const auto& idx = pm->GetParameterIndices("VfxMeshTrail");
            UpdateTrailCBV(previewTimer_);

            auto* texMgr = TextureManager::GetInstance();

            // t0: gTexNoise — noiseTexturePath が空ならホワイトテクスチャでフォールバック
            D3D12_GPU_DESCRIPTOR_HANDLE hNoise =
                (!asset.trail.noiseTexturePath.empty())
                ? texMgr->GetsrvHandleGPU(asset.trail.noiseTexturePath)
                : texMgr->GetsrvHandleGPU("Resources/Textures/white1x1.png");

            // t1: gTexRamp — texturePath が空なら同様にフォールバック
            D3D12_GPU_DESCRIPTOR_HANDLE hRamp =
                (!asset.trail.texturePath.empty())
                ? texMgr->GetsrvHandleGPU(asset.trail.texturePath)
                : texMgr->GetsrvHandleGPU("Resources/Textures/white1x1.png");

            cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshTrail"));
            cmdList->SetPipelineState(pm->GetBlendModePSO("VfxMeshTrail", asset.trail.blendMode));
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), cameraGPUAddress);
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), trailCBResource_->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootDescriptorTable(idx.at("gTexNoise"), hNoise); // t0
            cmdList->SetGraphicsRootDescriptorTable(idx.at("gTexRamp"), hRamp);  // t1

            previewTrail_->Draw(cmdList);
        }

        if (asset.useLightVolume) {
            const auto& idx = pm->GetParameterIndices("VfxMeshVolume");
            UpdateVolumeCBV(previewTimer_);

            cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshVolume"));
            cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshVolume"));
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), cameraGPUAddress);
            cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), volumeCBResource_->GetGPUVirtualAddress());

            previewVolume_->Draw(cmdList);
        }
    }

    // ===========================================================
    // ImGui メインウィンドウ
    // ===========================================================
    void VfxMeshEditor::DrawImGui()
    {
        history_.HandleKeyInput();

        ImGui::SetNextWindowSize(ImVec2(760, 680), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("VFX Mesh Editor")) { ImGui::End(); return; }

        // 左右分割レイアウト
        // 左: エフェクト一覧 (幅 200px 固定)
        ImGui::BeginChild("##list", ImVec2(200, 0), true);
        DrawListPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        // 右: パラメータ編集 + プレビュー
        ImGui::BeginChild("##edit", ImVec2(0, 0), true);
        DrawEditPanel();
        ImGui::EndChild();

        DrawNewEffectDialog();

        ImGui::End();
    }

    // ===========================================================
    // 左パネル: エフェクト一覧
    // ===========================================================
    void VfxMeshEditor::DrawListPanel()
    {
        // ヘッダー + 新規作成ボタン
        ImGui::TextDisabled("エフェクト一覧");
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) { showNewDialog_ = true; }
        ImGui::SameLine();
        // 再スキャンボタン
        if (ImGui::SmallButton("R")) {
            int prevSel = selectedIndex_;
            ScanDirectory(scanRoot_);
            // 再スキャン後に選択を復元 (同じパスが残っていれば)
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

        // エフェクト一覧
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            const auto& e = entries_[i];

            // dirty マークを名前に付加
            std::string label = e.asset.name;
            if (e.isDirty) label += " *";

            bool selected = (selectedIndex_ == i);
            if (ImGui::Selectable(label.c_str(), selected,
                ImGuiSelectableFlags_AllowDoubleClick)) {
                SelectEffect(i);
            }

            // 右クリックコンテキストメニュー
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("保存")) {
                    selectedIndex_ = i;
                    SaveCurrent();
                }
                if (ImGui::MenuItem("削除")) {
                    selectedIndex_ = i;
                    DeleteCurrent();
                    ImGui::EndPopup();
                    break; // entries_ が変わったのでループを抜ける
                }
                ImGui::EndPopup();
            }

            // ツールチップにファイルパスを表示
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", e.filePath.c_str());
            }
        }

        if (entries_.empty()) {
            ImGui::TextDisabled("(エフェクトなし)");
        }
    }

    // ===========================================================
    // 右パネル: パラメータ編集
    // ===========================================================
    void VfxMeshEditor::DrawEditPanel()
    {
        auto* sel = Selected();
        if (!sel) {
            ImGui::TextDisabled("エフェクトを選択してください");
            return;
        }
        auto& asset = sel->asset;

        // ---- ツールバー ----
        history_.DrawImGui();
        ImGui::SameLine();
        if (ImGui::Button("保存")) SaveCurrent();
        if (sel->isDirty) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.1f, 1.f), "* 未保存");
        }
        ImGui::Separator();

        // ---- ファイルパス (表示のみ) ----
        ImGui::TextDisabled("Path: %s", sel->filePath.c_str());
        ImGui::Separator();

        // ---- エフェクト名 ----
        strncpy_s(nameBuffer_, asset.name.c_str(), sizeof(nameBuffer_));
        ImGui::SetNextItemWidth(-1);
        VfxEffectAsset before = asset;
        if (ImGui::InputText("##effectname", nameBuffer_, sizeof(nameBuffer_),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            asset.name = nameBuffer_;
            CommitChange(before, "名前変更");
        }
        ImGui::SameLine(0, 4); ImGui::TextDisabled("名前");
        ImGui::Separator();

        // ---- Trail ----
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

        // ---- LightVolume ----
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

    // ===========================================================
    // Trail パラメータ編集
    // ===========================================================
    void VfxMeshEditor::DrawTrailSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& t = sel->asset.trail;

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
            c |= ImGui::SliderInt("最大ポイント##mp", &t.maxPoints, 4, 128);
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
            // ---------- ランプテクスチャ (t1: gTexRamp) ----------
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

            // ---------- ノイズテクスチャ (t0: gTexNoise) ----------
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

    // ===========================================================
    // テクスチャ選択ポップアップ
    // YParticleSystem::ShowEditor と同じ構造:
    //   showXxxPopup_ が true のとき OpenPopup → BeginPopupModal → Draw
    // ===========================================================
    void VfxMeshEditor::DrawTextureSelectPopup()
    {
        // ---- ランプテクスチャ (t1: gTexRamp) ----
        if (showRampPopup_) ImGui::OpenPopup("##RampTexSelect");

        ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("##RampTexSelect", &showRampPopup_,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
        {
            ImGui::Text("ランプテクスチャを選択 (t1: gTexRamp)");
            ImGui::Separator();
            // Grid モードで表示 — クリックでコールバックが発火し自動クローズ
            rampBrowser_.Draw("##RampBrowserChild", ImVec2(0, 340));
            ImGui::Separator();
            if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
                showRampPopup_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // ---- ノイズテクスチャ (t0: gTexNoise) ----
        if (showNoisePopup_) ImGui::OpenPopup("##NoiseTexSelect");

        ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("##NoiseTexSelect", &showNoisePopup_,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
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

    // ===========================================================
    // LightVolume パラメータ編集
    // ===========================================================
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

    // ===========================================================
    // プレビューセクション
    // ===========================================================
    void VfxMeshEditor::DrawPreviewSection()
    {
        ImGui::SeparatorText("プレビュー");

        if (previewPlaying_) {
            if (ImGui::Button("■ 停止")) {
                previewPlaying_ = false;
                previewTrail_->Clear();
            }
        }
        else {
            if (ImGui::Button("▶ 再生")) {
                previewPlaying_ = true;
                previewTimer_ = 0.f;
                previewTrail_->Clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("↺ リセット")) {
            previewTimer_ = 0.f;
            previewTrail_->Clear();
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

    // ===========================================================
    // 新規作成ダイアログ
    // ===========================================================
    void VfxMeshEditor::DrawNewEffectDialog()
    {
        if (!showNewDialog_) return;

        ImGui::OpenPopup("新規エフェクト作成");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal("新規エフェクト作成", &showNewDialog_,
            ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("エフェクト名");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newname", newNameBuffer_, sizeof(newNameBuffer_));

        ImGui::Spacing();
        ImGui::Text("保存先 JSON パス");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newpath", newPathBuffer_, sizeof(newPathBuffer_));

        // パスが空の場合はスキャンルート + 名前で自動補完
        if (newPathBuffer_[0] == '\0' && newNameBuffer_[0] != '\0') {
            std::string autoPath = scanRoot_ + newNameBuffer_ + ".json";
            ImGui::TextDisabled("→ %s", autoPath.c_str());
        }

        ImGui::Spacing();
        ImGui::Text("プリセット");
        const char* presetNames[] = { "Blank", "Trail Only", "Volume Only", "Sword (剣閃)", "Magic (魔法陣)" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##preset", &newPresetIdx_, presetNames, IM_ARRAYSIZE(presetNames));

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Button("作成", ImVec2(120, 0))) {
            // パスが空なら自動生成
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

    // ===========================================================
    // エフェクト操作
    // ===========================================================
    void VfxMeshEditor::SaveCurrent()
    {
        auto* sel = Selected();
        if (!sel) return;

        // ディレクトリが存在しなければ作成
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

        // ファイルを削除
        std::error_code ec;
        fs::remove(sel->filePath, ec);
        if (ec) Logger("VfxMeshEditor: 削除失敗 -> " + sel->filePath);
        else    Logger("VfxMeshEditor: 削除 -> " + sel->filePath);

        entries_.erase(entries_.begin() + selectedIndex_);

        // 選択を調整
        if (entries_.empty()) {
            selectedIndex_ = -1;
        }
        else {
            selectedIndex_ = std::min(selectedIndex_,
                static_cast<int>(entries_.size()) - 1);
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

        // 新規作成したエフェクトを選択して即保存
        SelectEffect(static_cast<int>(entries_.size()) - 1);
        SaveCurrent();

        Logger("VfxMeshEditor: 新規作成 -> " + filePath);
    }

    // ===========================================================
    // CommitChange
    // ===========================================================
    void VfxMeshEditor::CommitChange(const VfxEffectAsset& before, const char* label)
    {
        auto* sel = Selected();
        if (!sel) return;

        VfxEffectAsset after = sel->asset;
        const int idx = selectedIndex_;

        history_.Execute(MakeLambdaCommand(
            label,
            [this, idx, after]() {  // Redo
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = after;
                    entries_[idx].isDirty = true;
                    previewTrail_->ApplyParam(after.trail);
                    previewVolume_->ApplyParam(after.lightVolume);
                }
            },
            [this, idx, before]() { // Undo
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = before;
                    entries_[idx].isDirty = true;
                    previewTrail_->ApplyParam(before.trail);
                    previewVolume_->ApplyParam(before.lightVolume);
                }
            }
        ));

        sel->isDirty = true;
    }

    // ===========================================================
    // プレビューメッシュ再構築
    // ===========================================================
    void VfxMeshEditor::RebuildPreviewMeshes()
    {
        auto* sel = Selected();
        if (!sel) return;
        previewTrail_->Initialize(sel->asset.trail);
        previewVolume_->Initialize(sel->asset.lightVolume);
    }

    // ===========================================================
    // CBV
    // ===========================================================
    void VfxMeshEditor::InitCBVs()
    {
        trailCBResource_ = dxCommon_->CreateBufferResource(AlignedSize<MeshTrailParamsCB>());
        trailCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&trailCBMapped_));

        volumeCBResource_ = dxCommon_->CreateBufferResource(AlignedSize<LightVolumeParamsCB>());
        volumeCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&volumeCBMapped_));
    }

    void VfxMeshEditor::UpdateTrailCBV(float time)
    {
        auto* sel = Selected();
        if (!sel || !trailCBMapped_) return;
        const auto& t = sel->asset.trail;

        trailCBMapped_->colorInner[0] = t.colorEnd.x;
        trailCBMapped_->colorInner[1] = t.colorEnd.y;
        trailCBMapped_->colorInner[2] = t.colorEnd.z;
        trailCBMapped_->colorInner[3] = t.colorEnd.w;
        trailCBMapped_->colorOuter[0] = t.colorStart.x;
        trailCBMapped_->colorOuter[1] = t.colorStart.y;
        trailCBMapped_->colorOuter[2] = t.colorStart.z;
        trailCBMapped_->colorOuter[3] = t.colorStart.w;
        trailCBMapped_->softness = 0.15f;
        trailCBMapped_->glowPower = 1.5f;
        trailCBMapped_->distortion = 0.0f;
        trailCBMapped_->time = time;
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

    // ===========================================================
    // プリセット
    // ===========================================================
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