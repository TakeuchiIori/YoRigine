// ===========================================================
// VfxMeshEditor.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "VfxMeshEditor.h"
#include "DirectXCommon.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include "Debugger/Logger.h"
#include <Loaders/Texture/TextureManager.h>
#include <IconsFontAwesome5.h>


namespace fs = std::filesystem;

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

        // Trail用のプレビューはEmitterに委譲
        previewTrailEmitter_ = std::make_unique<TrailMeshEmitter>();

        ScanDirectory(scanRoot_);

        if (!entries_.empty()) {
            SelectEffect(0);
        }
    }

    void VfxMeshEditor::Finalize()
    {
        previewSubs_.clear();   // ~PreviewSub が CB を Unmap する
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

        // Emitter に現在のアセットを反映させる
        if (previewTrailEmitter_) {
            previewTrailEmitter_->Stop();
            previewTrailEmitter_->SetCamera(camera_);
            previewTrailEmitter_->LoadAsset(entries_[selectedIndex_].filePath);
            previewTrailEmitter_->SetAsset(entries_[selectedIndex_].asset); // 最新状態を同期
        }

        previewTrailEmitter_->Play();

        SyncPreviewSubs();
    }

        // ===========================================================
    // 描画（cmdListを受け取らず、Emitter側のDrawを呼ぶ）
    // ===========================================================
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
            // 名前に合わせて JSON ファイル自体もリネームする
            RenameCurrentFile(nameBuffer_);
        }
        ImGui::SameLine(0, 4); ImGui::TextDisabled("名前");
        ImGui::Separator();

        // タブ構成: Trail（Emitter駆動で1つ） / 形状（サブ効果リスト） / Motion（全体の動き）
        if (ImGui::BeginTabBar("##vfxTabs")) {
            // Trail タブ
            {
                // "###tab_xxx" でタブ ID を固定（アイコンの付け外しで選択がリセットされないように）
                std::string label = std::string(asset.useTrail ? (ICON_FA_CIRCLE " ") : "   ") + "Trail###tab_Trail";
                if (ImGui::BeginTabItem(label.c_str())) {
                    VfxEffectAsset b = asset;
                    if (ImGui::Checkbox("この効果を有効化", &asset.useTrail)) CommitChange(b, "Trail 有効切替");
                    ImGui::Separator();
                    if (asset.useTrail) DrawTrailSection();
                    else                ImGui::TextDisabled("(無効) — 上のチェックで有効化");
                    ImGui::EndTabItem();
                }
            }

            // 形状タブ（サブ効果リスト: 同じ種類を複数積める）
            {
                std::string slabel = std::string(asset.subEffects.empty() ? "   " : (ICON_FA_CIRCLE " "))
                                   + "形状 (" + std::to_string(asset.subEffects.size()) + ")###tab_SubEffects";
                if (ImGui::BeginTabItem(slabel.c_str())) {
                    DrawSubEffectsSection();
                    ImGui::EndTabItem();
                }
            }

            // Motion タブ（エフェクト全体の動きのリストを編集）
            {
                std::string mlabel = std::string(asset.motions.empty() ? "   " : (ICON_FA_CIRCLE " "))
                                   + "Motion###tab_Motion";
                if (ImGui::BeginTabItem(mlabel.c_str())) {
                    DrawMotionSection();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::Separator();

        DrawPreviewSection();
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
        const char* presetNames[] = { "Blank", "Trail Only", "Volume Only", "Waypoint Beam", "Sword", "Magic", "Explosion (モーション駆動)" };
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

} // namespace YoRigine
#endif