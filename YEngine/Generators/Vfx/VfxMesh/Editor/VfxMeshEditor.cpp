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

        // Primitive (3D) 専用セクション
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
            const ImGuiColorEditFlags hdr = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;
            c |= ImGui::ColorEdit4("根元カラー##cs", &t.colorStart.x, hdr);
            c |= ImGui::ColorEdit4("先端カラー##ce", &t.colorEnd.x, hdr);
            if (c) CommitChange(b, "Trail カラー");
            ImGui::TextDisabled("  ※値を >1 にすると Bloom で強く発光する");
        }

        ImGui::SeparatorText("発光");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("発光強度 (0で消灯)##emi", &t.emissiveIntensity, 0.f, 5.f, "%.2f");
            c |= ImGui::SliderFloat("中心グロー##glow", &t.glowPower, 0.f, 8.f, "%.2f");
            c |= ImGui::SliderFloat("エッジソフト##sof", &t.softness, 0.f, 1.f, "%.2f");
            if (c) CommitChange(b, "Trail 発光");
            ImGui::TextDisabled("  ※発光強度=0 で完全に消灯（形だけ確認したい時に）");
        }

        ImGui::SeparatorText("アウトライン強調 (縁の発光)");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::ColorEdit4("縁の色##rim", &t.rimColor.x,
                                   ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            c |= ImGui::SliderFloat("縁の強さ##rim",  &t.fresnelStrength, 0.f, 5.f, "%.2f");
            c |= ImGui::SliderFloat("縁の細さ##rim",  &t.trailSharpness,  0.2f, 8.f, "%.2f");
            if (c) CommitChange(b, "Trail アウトライン");
            ImGui::TextDisabled("  ※縁を HDR(>1) にすると Bloom で強く光る");
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
        const char* presetNames[] = { "Blank", "Trail Only", "Volume Only", "Sword", "Magic", "Explosion (モーション駆動)" };
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
