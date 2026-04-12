// ===========================================================
// VfxMeshEditor.cpp
// ===========================================================
#include "VfxMeshEditor.h"
#include <imgui.h>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "Debugger/Logger.h"

namespace fs = std::filesystem;

namespace YoRigine {

    // ===========================================================
    // コンストラクタ
    // ===========================================================
    VfxMeshEditor::VfxMeshEditor(const std::string& jsonPath) {
        lastCheckTime_ = std::chrono::steady_clock::now();

        // プレビュー用メッシュを生成 (パラメータは後で RebuildMeshes で適用)
        previewTrail_ = std::make_unique<TrailMesh>();
        previewVolume_ = std::make_unique<LightVolumeMesh>();

        if (!jsonPath.empty()) {
            Load(jsonPath);          // Load 内で RebuildMeshes を呼ぶ
        }
        else {
            RebuildMeshes();         // デフォルトパラメータで初期化
        }
    }

    // ===========================================================
    // 毎フレーム
    // ===========================================================
    void VfxMeshEditor::Update(float deltaTime) {
        if (hotReload_) PollFileChange();

        if (!previewPlaying_) return;

        previewTimer_ += deltaTime;

        // Trail プレビュー: 原点から ±Y 方向に往復する仮ポイントを追加
        if (asset_.useTrail) {
            previewTrail_->ApplyParam(asset_.trail);

            // 単純な往復アニメーション (周期 = lifetime * 2)
            const float period = std::max(asset_.trail.lifetime * 2.f, 0.2f);
            const float t = std::fmod(previewTimer_, period) / period; // 0→1
            const float swing = std::sin(t * 3.14159f * 2.f);

            const Vector3 tip = {
                previewCenter_.x + swing * 0.5f,
                previewCenter_.y + 1.0f,
                previewCenter_.z
            };
            const Vector3 root = {
                previewCenter_.x + swing * 0.5f,
                previewCenter_.y - 1.0f,
                previewCenter_.z
            };
            previewTrail_->AddPoint(tip, root);
            previewTrail_->Update(deltaTime);
        }

        // LightVolume プレビュー: previewCenter_ + previewYaw_ でそのまま配置
        if (asset_.useLightVolume) {
            previewVolume_->ApplyParam(asset_.lightVolume);
            previewVolume_->SetTransform(previewCenter_, previewYaw_);
            previewVolume_->Update(deltaTime);
        }
    }

    // ===========================================================
    // プレビュー描画 (ゲームの Draw パスと同じ場所で呼ぶ)
    // ===========================================================
    void VfxMeshEditor::DrawPreview(ID3D12GraphicsCommandList* cmdList,
        D3D12_GPU_VIRTUAL_ADDRESS ) {
        if (!previewPlaying_) return;

        if (asset_.useTrail) {
            previewTrail_->Draw(cmdList);
        }
        if (asset_.useLightVolume) {
            previewVolume_->Draw(cmdList);
        }
    }

    // ===========================================================
    // メイン ImGui ウィンドウ
    // ===========================================================
    void VfxMeshEditor::DrawImGui() {
        ImGui::SetNextWindowSize(ImVec2(440, 760), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("VFX Mesh Editor")) { ImGui::End(); return; }

        // Undo/Redo ショートカット & ボタンは CommandHistory に委譲
        history_.HandleKeyInput();

        DrawToolbar();
        ImGui::Separator();
        DrawFileSection();
        ImGui::Separator();
        DrawNameSection();
        ImGui::Separator();

        // ---- Trail ----
        {
            VfxEffectAsset before = asset_;
            ImGui::PushID("UseTrail");
            bool changed = ImGui::Checkbox("##UseTrail", &asset_.useTrail);
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNodeEx("Trail", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (asset_.useTrail) DrawTrailSection();
                else                 ImGui::TextDisabled("  (disabled)");
                ImGui::TreePop();
            }
            if (changed) CommitChange(before, "Trail 有効切替");
        }
        ImGui::Separator();

        // ---- LightVolume ----
        {
            VfxEffectAsset before = asset_;
            ImGui::PushID("UseLightVolume");
            bool changed = ImGui::Checkbox("##UseLightVolume", &asset_.useLightVolume);
            ImGui::PopID();
            ImGui::SameLine();
            if (ImGui::TreeNodeEx("Light Volume", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (asset_.useLightVolume) DrawLightVolumeSection();
                else                       ImGui::TextDisabled("  (disabled)");
                ImGui::TreePop();
            }
            if (changed) CommitChange(before, "Volume 有効切替");
        }
        ImGui::Separator();

        DrawPreviewSection();

        if (autoSave_ && isDirty_) Save();

        DrawNewEffectDialog();

        ImGui::End();
    }

    // ===========================================================
    // ツールバー
    // ===========================================================
    void VfxMeshEditor::DrawToolbar() {
        if (ImGui::Button("New")) { showNewDialog_ = true; }
        ImGui::SameLine();

        if (ImGui::Button("Open")) {
            if (browseCallback_) {
                std::string picked;
                browseCallback_(picked);
                if (!picked.empty()) Load(picked);
            }
        }
        ImGui::SameLine();

        if (currentPath_.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Save")) Save();
        if (currentPath_.empty()) ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Save As")) {
            if (browseCallback_) {
                std::string picked;
                browseCallback_(picked);
                if (!picked.empty()) Save(picked);
            }
        }
        ImGui::SameLine();

        ImGui::Spacing(); ImGui::SameLine();

        // Undo/Redo ボタン + 履歴ツリーは CommandHistory::DrawImGui() に任せる
        history_.DrawImGui();

        if (isDirty_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.1f, 1.f), "* 未保存");
        }
    }

    // ===========================================================
    // ファイルセクション
    // ===========================================================
    void VfxMeshEditor::DrawFileSection() {
        ImGui::TextDisabled("Path: ");
        ImGui::SameLine();
        if (currentPath_.empty()) ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.f), "(未保存)");
        else                      ImGui::TextUnformatted(currentPath_.c_str());

        ImGui::Checkbox("Auto Save", &autoSave_);
        ImGui::SameLine();
        ImGui::Checkbox("Hot Reload", &hotReload_);
    }

    // ===========================================================
    // 新規作成ダイアログ
    // ===========================================================
    void VfxMeshEditor::DrawNewEffectDialog() {
        if (!showNewDialog_) return;

        ImGui::OpenPopup("新規エフェクト作成");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal("新規エフェクト作成", &showNewDialog_,
            ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("エフェクト名");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newname", newNameBuffer_, sizeof(newNameBuffer_));

        ImGui::Spacing(); ImGui::Text("保存先 JSON パス");
        ImGui::SetNextItemWidth(browseCallback_ ? 300.0f : -1.0f);
        ImGui::InputText("##newpath", newPathBuffer_, sizeof(newPathBuffer_));
        if (browseCallback_) {
            ImGui::SameLine();
            if (ImGui::Button("...")) {
                std::string picked; browseCallback_(picked);
                if (!picked.empty()) strncpy_s(newPathBuffer_, picked.c_str(), sizeof(newPathBuffer_));
            }
        }

        ImGui::Spacing(); ImGui::Text("プリセット");
        const char* presetNames[] = { "Blank", "Trail Only", "Volume Only", "Sword (剣閃)", "Magic (魔法陣)" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##preset", &newPresetIdx_, presetNames, IM_ARRAYSIZE(presetNames));

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        bool pathOk = (newPathBuffer_[0] != '\0');
        if (!pathOk) ImGui::BeginDisabled();
        if (ImGui::Button("作成", ImVec2(120, 0))) {
            asset_ = MakePreset(static_cast<VfxPreset>(newPresetIdx_));
            asset_.name = newNameBuffer_;
            currentPath_ = newPathBuffer_;
            isDirty_ = true;
            showNewDialog_ = false;
            history_.Clear();
            RebuildMeshes();
            Save(currentPath_);
            if (onChangedCallback_) onChangedCallback_(asset_);
            ImGui::CloseCurrentPopup();
        }
        if (!pathOk) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
            showNewDialog_ = false; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ===========================================================
    // エフェクト名
    // ===========================================================
    void VfxMeshEditor::DrawNameSection() {
        strncpy_s(nameBuffer_, asset_.name.c_str(), sizeof(nameBuffer_));
        ImGui::SetNextItemWidth(-1);
        VfxEffectAsset before = asset_;
        if (ImGui::InputText("##effectname", nameBuffer_, sizeof(nameBuffer_),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            asset_.name = nameBuffer_;
            CommitChange(before, "名前変更");
        }
        ImGui::SameLine(0, 4); ImGui::TextDisabled("名前");
    }

    // ===========================================================
    // Trail セクション
    // ===========================================================
    void VfxMeshEditor::DrawTrailSection() {
        auto& t = asset_.trail;

        ImGui::SeparatorText("形状");
        {
            VfxEffectAsset before = asset_;
            bool c = false;
            const char* shapeNames[] = { "Flat", "Arc", "Fan" };
            int shapeIdx = static_cast<int>(t.shapeType);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##shapeType", &shapeIdx, shapeNames, IM_ARRAYSIZE(shapeNames))) {
                t.shapeType = static_cast<TrailShapeType>(shapeIdx);
                c = true;
            }
            c |= ImGui::SliderInt("幅分割数##ws", &t.widthSegments, 1, 16);
            if (t.shapeType != TrailShapeType::Flat)
                c |= ImGui::SliderFloat("弧角度 (度)##arc", &t.arcAngleDeg, 10.0f, 360.0f, "%.1f");
            if (c) CommitChange(before, "Trail 形状");
        }

        ImGui::SeparatorText("幅");
        {
            VfxEffectAsset before = asset_;
            bool c = false;
            c |= ImGui::SliderFloat("根元幅##ws", &t.widthStart, 0.0f, 5.0f, "%.3f");
            c |= ImGui::SliderFloat("先端幅##we", &t.widthEnd, 0.0f, 5.0f, "%.3f");
            if (c) CommitChange(before, "Trail 幅");
        }

        ImGui::SeparatorText("寿命 / ポイント数");
        {
            VfxEffectAsset before = asset_;
            bool c = false;
            c |= ImGui::SliderFloat("寿命 (秒)##lt", &t.lifetime, 0.05f, 5.0f, "%.2f");
            c |= ImGui::SliderInt("最大ポイント##mp", &t.maxPoints, 4, 128);
            if (c) CommitChange(before, "Trail 寿命");
        }

        ImGui::SeparatorText("カラーグラデーション");
        {
            VfxEffectAsset before = asset_;
            bool c = false;
            c |= ImGui::ColorEdit4("根元カラー##cs", &t.colorStart.x);
            c |= ImGui::ColorEdit4("先端カラー##ce", &t.colorEnd.x);
            if (c) CommitChange(before, "Trail カラー");
        }

        ImGui::SeparatorText("ブレンドモード");
        {
            VfxEffectAsset before = asset_;
            const char* names[] = { "通常", "加算", "減算", "乗算" };
            int idx = static_cast<int>(t.blendMode);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##blend", &idx, names, IM_ARRAYSIZE(names))) {
                t.blendMode = static_cast<BlendMode>(idx);
                CommitChange(before, "Trail ブレンド");
            }
        }

        ImGui::SeparatorText("シェーダー");
        {
            VfxEffectAsset before = asset_;
            if (ImGui::SliderFloat("UV スクロール速度##uvs", &t.uvScrollSpeed, -5.f, 5.f, "%.2f"))
                CommitChange(before, "Trail UV スクロール");
        }

        ImGui::SeparatorText("テクスチャ");
        {
            strncpy_s(textureBuffer_, t.texturePath.c_str(), sizeof(textureBuffer_));
            float bw = browseCallback_ ? 100.f : 52.f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - bw - 8);
            VfxEffectAsset before = asset_;
            if (ImGui::InputText("##texpath", textureBuffer_, sizeof(textureBuffer_))) {
                t.texturePath = textureBuffer_; CommitChange(before, "Trail テクスチャ");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("クリア")) {
                before = asset_; t.texturePath.clear(); CommitChange(before, "Trail テクスチャクリア");
            }
            if (browseCallback_) {
                ImGui::SameLine();
                if (ImGui::SmallButton("参照")) {
                    std::string picked; browseCallback_(picked);
                    if (!picked.empty()) {
                        before = asset_; t.texturePath = picked;
                        CommitChange(before, "Trail テクスチャ参照");
                    }
                }
            }
        }

        ImGui::Spacing();
        ImGui::Checkbox("Trail デバッグ表示", &showTrailDebug_);
        if (showTrailDebug_) ImGui::TextColored(ImVec4(1, 1, 0, 1), "  > デバッグオーバーレイ ON");
    }

    // ===========================================================
    // LightVolume セクション
    // ===========================================================
    void VfxMeshEditor::DrawLightVolumeSection() {
        auto& lv = asset_.lightVolume;

        ImGui::SeparatorText("OBB サイズ  (X=右 / Y=上 / Z=前)");
        {
            VfxEffectAsset before = asset_;
            if (ImGui::DragFloat3("半辺長##he", &lv.halfExtents.x, 0.05f, 0.01f, 50.f, "%.2f"))
                CommitChange(before, "Volume サイズ");
        }

        ImGui::SeparatorText("カラー  (α = ボリューム濃度)");
        {
            VfxEffectAsset before = asset_;
            bool c = false;
            c |= ImGui::ColorEdit4("ボリュームカラー##vc", &lv.color.x);
            c |= ImGui::SliderFloat("輝度##inten", &lv.intensity, 0.f, 10.f, "%.2f");
            if (c) CommitChange(before, "Volume カラー");
        }

        ImGui::Spacing();
        {
            VfxEffectAsset before = asset_;
            if (ImGui::Checkbox("ボリューム有効##en", &lv.isEnable))
                CommitChange(before, "Volume 有効切替");
        }

        ImGui::Spacing();
        ImGui::Checkbox("OBB ワイヤーフレーム表示", &showVolumeDebug_);
        if (showVolumeDebug_) ImGui::TextColored(ImVec4(1, 1, 0, 1), "  > OBB ワイヤーフレーム ON");
    }

    // ===========================================================
    // プレビューセクション
    // ===========================================================
    void VfxMeshEditor::DrawPreviewSection() {
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

        // 位置・回転 調整スライダー
        ImGui::DragFloat3("プレビュー位置", &previewCenter_.x, 0.05f, -20.f, 20.f);
        ImGui::SliderAngle("プレビューYaw", &previewYaw_, -180.f, 180.f);

        // Trail タイムバー
        if (asset_.useTrail && asset_.trail.lifetime > 0.f) {
            char ov[48];
            snprintf(ov, sizeof(ov), "Trail  %.2f / %.2f s",
                previewTimer_, asset_.trail.lifetime);
            ImGui::ProgressBar(
                std::min(previewTimer_ / asset_.trail.lifetime, 1.f),
                ImVec2(-1, 0), ov);
        }

        // Volume インジケータ
        if (asset_.useLightVolume && asset_.lightVolume.isEnable) {
            ImGui::ProgressBar(1.f, ImVec2(-1, 0), "Light Volume  (active)");
        }

        ImGui::TextDisabled("時刻: %.3f s", previewTimer_);
    }

    // ===========================================================
    // CommitChange — 変更を CommandHistory に登録してDirtyマーク
    // ===========================================================
    void VfxMeshEditor::CommitChange(const VfxEffectAsset& before, const char* label) {
        VfxEffectAsset after = asset_;  // 変更後

        history_.Execute(MakeLambdaCommand(
            label,
            // Execute: after を適用 (Redo 時に使われる)
            [this, after]() {
                asset_ = after;
                MarkDirty();
                previewTrail_->ApplyParam(asset_.trail);
                previewVolume_->ApplyParam(asset_.lightVolume);
            },
            // Undo: before に戻す
            [this, before]() {
                asset_ = before;
                MarkDirty();
                previewTrail_->ApplyParam(asset_.trail);
                previewVolume_->ApplyParam(asset_.lightVolume);
            }
        ));

        MarkDirty();
        // Execute はコマンド内で asset_ を after に再セットするが、
        // ここでは ImGui がすでに asset_ を書き換えた後なので
        // Execute の中身は Redo 専用として機能する
    }

    void VfxMeshEditor::MarkDirty() {
        isDirty_ = true;
        if (onChangedCallback_) onChangedCallback_(asset_);
    }

    // ===========================================================
    // RebuildMeshes — アセットパラメータでメッシュを再初期化
    // ===========================================================
    void VfxMeshEditor::RebuildMeshes() {
        previewTrail_->Initialize(asset_.trail);
        previewVolume_->Initialize(asset_.lightVolume);
    }

    // ===========================================================
    // Save / Load
    // ===========================================================
    void VfxMeshEditor::Save(const std::string& path) {
        const std::string& target = path.empty() ? currentPath_ : path;
        if (target.empty()) { Logger("VfxMeshEditor: 保存パスが空です"); return; }

        fs::path p(target);
        if (!p.parent_path().empty()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
        asset_.SaveToJson(target);
        currentPath_ = target;
        lastFileTime_ = GetFileTime(currentPath_);
        isDirty_ = false;
        Logger("VfxMeshEditor: 保存 -> " + currentPath_);
    }

    bool VfxMeshEditor::Load(const std::string& path) {
        if (!asset_.LoadFromJson(path)) return false;
        currentPath_ = path;
        lastFileTime_ = GetFileTime(path);
        isDirty_ = false;
        history_.Clear();
        RebuildMeshes();
        if (onChangedCallback_) onChangedCallback_(asset_);
        Logger("VfxMeshEditor: ロード -> " + currentPath_);
        return true;
    }

    // ===========================================================
    // ホットリロード
    // ===========================================================
    void VfxMeshEditor::PollFileChange() {
        if (currentPath_.empty()) return;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - lastCheckTime_).count() < pollIntervalSec_) return;
        lastCheckTime_ = now;

        auto t = GetFileTime(currentPath_);
        if (t != lastFileTime_) {
            Logger("VfxMeshEditor: ホットリロード -> " + currentPath_);
            Load(currentPath_);
        }
    }

    std::filesystem::file_time_type VfxMeshEditor::GetFileTime(const std::string& path) const {
        std::error_code ec;
        auto t = fs::last_write_time(path, ec);
        return ec ? fs::file_time_type{} : t;
    }

    // ===========================================================
    // プリセット
    // ===========================================================
    VfxEffectAsset VfxMeshEditor::MakePreset(VfxPreset preset) {
        VfxEffectAsset a;
        switch (preset) {
        case VfxPreset::TrailOnly:
            a.useTrail = true; a.useLightVolume = false; break;
        case VfxPreset::VolumeOnly:
            a.useTrail = false; a.useLightVolume = true; break;
        case VfxPreset::Sword:
            a.useTrail = true; a.useLightVolume = true;
            a.trail.widthStart = 0.05f; a.trail.widthEnd = 0.f;
            a.trail.lifetime = 0.25f;   a.trail.maxPoints = 48;
            a.trail.colorStart = { 1.f,0.95f,0.7f,1.f };
            a.trail.colorEnd = { 0.8f,0.5f,0.1f,0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.3f;
            a.lightVolume.halfExtents = { 0.04f,0.04f,0.6f };
            a.lightVolume.color = { 1.f,0.9f,0.5f,0.18f };
            a.lightVolume.intensity = 1.5f;
            break;
        case VfxPreset::Magic:
            a.useTrail = true; a.useLightVolume = true;
            a.trail.widthStart = 0.4f;  a.trail.widthEnd = 0.05f;
            a.trail.lifetime = 1.2f;    a.trail.maxPoints = 96;
            a.trail.colorStart = { 0.4f,0.2f,1.f,1.f };
            a.trail.colorEnd = { 0.2f,0.6f,1.f,0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.8f;
            a.lightVolume.halfExtents = { 1.5f,1.5f,4.f };
            a.lightVolume.color = { 0.5f,0.3f,1.f,0.25f };
            a.lightVolume.intensity = 3.f;
            break;
        default: break;
        }
        return a;
    }

} // namespace YoRigine