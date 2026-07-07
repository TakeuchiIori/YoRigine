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
#include <IconsFontAwesome5.h>


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

    void VfxMeshEditor::Update(float deltaTime)
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        previewTimer_ += deltaTime;
        const auto& asset = sel->asset;

        // ワンショット寿命はモーション優先（BurstGrow が全体/形状個別にあればその最大 duration。
        // ゲーム側 OneShotDuration() と同じ考え方）。無ければ UI の burstDuration_ をフォールバック。
        float burstPeriod = burstDuration_;
        {
            float bg = 0.f;
            auto scan = [&bg](const std::vector<VfxMotion>& ms) {
                for (const auto& m : ms) {
                    if (m.type == VfxMotionType::BurstGrow) bg = std::max(bg, m.duration);
                }
            };
            scan(asset.motions);
            for (const auto& sub : asset.subEffects) scan(sub.motions);
            if (bg > 0.f) burstPeriod = std::max(bg, 0.01f);
        }

        // プレビューは常に「寿命ぶんでモーションを1回再生」する（＝パーティクルのワンショット相当）。
        //   loopOneShot_ ON  : 休止をはさんで毎サイクル まっさらに再Emit（1回ずつ繰り返し）
        //   loopOneShot_ OFF : 1回だけ再生して終端で停止（もう一度/リセットで再生）
        // 動きは全部モーション（ScaleOverLife/Rise/FadeInOut 等）で作る。ハードコードはしない。
        burstMode_ = (burstPeriod > 0.01f);
        oneShotLocal_ = 0.f;            // このサイクル内での経過（0 が一発の始まり）
        bool oneShotIdle = false;       // 「出しきって終わった」休止中か（ループの休止）
        if (burstMode_) {
            if (loopOneShot_) {
                const float gap   = std::max(oneShotGap_, 0.0f);
                const float cycle = burstPeriod + gap;
                oneShotLocal_ = std::fmod(previewTimer_, cycle);
                if (oneShotLocal_ >= burstPeriod) {
                    oneShotLocal_ = burstPeriod; // 終端で保持
                    oneShotIdle   = true;        // 休止：完全に非表示 → 次サイクルで再Emit
                }
            } else {
                oneShotLocal_ = std::min(previewTimer_, burstPeriod); // 1回で止まる
            }
            burstProgress_ = std::min(oneShotLocal_ / burstPeriod, 1.0f);
        } else {
            burstProgress_ = -1.0f;
        }
        const float oneShotLocal = oneShotLocal_; // 以降のローカル参照用

        // サブ効果の構成変更（追加/削除/種類変更/Undo）をプレビューへ反映
        SyncPreviewSubs();

        // データ駆動モーションのベース状態（サブ効果ごとに評価する）
        VfxEvalState mbase;
        // age は毎サイクル 0 から振り直す（＝毎回まっさらな1発。previewTimer_ をそのまま渡すと
        // 上昇/漂いが累積してしまう）。lifetime は寿命（FadeInOut などの終端計算に使う）。
        mbase.age      = burstMode_ ? oneShotLocal : previewTimer_;
        mbase.progress = burstProgress_;
        mbase.lifetime = burstMode_ ? burstPeriod : -1.f;
        mbase.position = previewCenter_;
        mbase.scale    = 1.0f;

        // ★ Emitterを使って頂点を更新
        if (asset.useTrail && previewTrailEmitter_) {
            Vector3 tip = previewCenter_;
            Vector3 root = previewCenter_;
            Vector3 widthDir = { 0.f, 1.f, 0.f };

            const float period = std::max(asset.trail.lifetime * 1.5f, 0.5f);

            // ワンショット: 1振りして手を止め、既存トレイルがフェード/ディゾルブで
            // 消えたら少し待って最初から。継続モードは従来どおりループ。
            bool  addThisFrame = true;
            float t;
            if (trailOneShot_) {
                const float swingDur = period / std::max(previewSpeed_, 0.01f);
                const float cycle    = swingDur + asset.trail.lifetime + 0.4f;
                const float local    = std::fmod(previewTimer_, cycle);
                if (local < deltaTime && previewTrailEmitter_)
                    previewTrailEmitter_->Play();          // サイクル頭で軌跡クリア＆再生
                if (local <= swingDur) {
                    t = std::min(local / swingDur, 1.0f);  // 0→1 で 1 振り
                } else {
                    t = 1.0f;                              // 振り終わり位置で固定
                    addThisFrame = false;                  // 以降は点を足さない → 自然に消える
                }
            } else {
                t = std::fmod(previewTimer_ * previewSpeed_, period) / period;
            }

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

            // ★ Emitterに点を追加（widthDirも渡す）。ワンショットの停止中は足さない。
            if (addThisFrame)
                previewTrailEmitter_->AddPoint(tip, root, widthDir);
            previewTrailEmitter_->Update(deltaTime);
        }

        // サブ効果ごとに: オフセット → モーション（全体＋個別） → 更新
        const size_t subCount = std::min(asset.subEffects.size(), previewSubs_.size());
        for (size_t i = 0; i < subCount; ++i) {
            const auto& def = asset.subEffects[i];
            auto&       sub = *previewSubs_[i];
            if (!def.enabled) continue;

            // ワンショットの休止中は「出しきって終わった」状態。完全に消す（描画側は visible を見る）。
            if (oneShotIdle) {
                sub.visible = false;
                continue;
            }

            VfxEvalState s = mbase;
            s.position += def.offset;
            EvaluateSubEffectMotions(asset, def, s);

            // 色乗算・表示状態は DrawPreview の CB 反映で使う
            sub.tint    = s.colorTint;
            sub.visible = s.visible;

            switch (def.type) {
            case VfxSubEffectType::LightVolume:
                if (sub.volume) {
                    sub.volume->ApplyParam(def.lightVolume);
                    sub.volume->SetTransform(s.position, previewYaw_);
                    sub.volume->Update(deltaTime);
                }
                break;

            case VfxSubEffectType::Smoke:
                if (sub.smoke) {
                    // 膨張/上昇の計算は VolumeSmokeMesh::Drive に集約（ゲーム側 Spawner と同一）
                    sub.smoke->ApplyParam(def.smoke);
                    sub.smoke->Drive(s);
                    sub.smoke->Update(deltaTime);
                    sub.smokeCenter = sub.smoke->GetCenter();  // CB 用に読み戻す
                    sub.smokeRadius = sub.smoke->GetRadius();
                }
                break;

            case VfxSubEffectType::Lightning:
                if (sub.lightning) {
                    sub.lightning->SetCamera(camera_);
                    sub.lightning->ApplyParam(def.lightning);
                    // プレビュー: 中心を挟んで direction 方向に length の長さで伸ばす（中心はモーションで移動）
                    float half = def.lightning.length * 0.5f;
                    Vector3 dir = def.lightning.direction;
                    float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                    dir = (dl < 1e-4f) ? Vector3{ 0, 1, 0 } : Vector3{ dir.x / dl, dir.y / dl, dir.z / dl };
                    sub.lightning->SetEndpoints(s.position - dir * half,
                                                s.position + dir * half);
                    sub.lightning->Update(deltaTime);
                }
                break;

            case VfxSubEffectType::Shockwave:
                if (sub.shockwave) {
                    sub.shockwave->SetCamera(camera_);
                    sub.shockwave->ApplyParam(def.shockwave);
                    sub.shockwave->Drive(s);
                    sub.shockwave->Update(deltaTime);
                }
                break;
            }
        }
    }

    // ===========================================================
    // 描画（cmdListを受け取らず、Emitter側のDrawを呼ぶ）
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

        if (!camera_) return;

        auto* pm      = YPipelineManager::GetInstance();
        auto* cmdList = DirectXCommon::GetInstance()->GetCommandList().Get();
        D3D12_GPU_VIRTUAL_ADDRESS camAddr = camera_->GetCameraResource()->GetGPUVirtualAddress();

        // モーションの色乗算（rgb=色 / a=不透明度）を CB 用カラーへ適用する
        auto tint4 = [](const Vector4& c, const Vector4& t) -> Vector4 {
            return { c.x * t.x, c.y * t.y, c.z * t.z, c.w * t.w };
        };

        // サブ効果ごとに CB 更新 → 描画
        const size_t subCount = std::min(asset.subEffects.size(), previewSubs_.size());
        for (size_t i = 0; i < subCount; ++i) {
            const auto& def = asset.subEffects[i];
            auto&       sub = *previewSubs_[i];
            if (!def.enabled || !sub.cbMapped || !sub.cbRes) continue;
            if (!sub.visible || sub.tint.w <= 0.001f) continue; // Visibility/フェードで消えている

            switch (def.type) {
            case VfxSubEffectType::LightVolume:
                if (sub.volume) {
                    const auto& lv = def.lightVolume;
                    auto& cb = *static_cast<LightVolumeParamsCB*>(sub.cbMapped);
                    cb.color[0] = lv.color.x * sub.tint.x;
                    cb.color[1] = lv.color.y * sub.tint.y;
                    cb.color[2] = lv.color.z * sub.tint.z;
                    cb.color[3] = lv.color.w * lv.intensity * sub.tint.w;
                    cb.edgeFade = 0.15f;
                    cb.depthFade = 1.0f;
                    cb.noiseTiling = 2.0f;
                    cb.noiseStrength = 0.0f;
                    cb.time = previewTimer_;

                    const auto& idx = pm->GetParameterIndices("VfxMeshVolume");
                    cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshVolume"));
                    cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshVolume"));
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camAddr);
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                    sub.volume->Draw(cmdList);
                }
                break;

            case VfxSubEffectType::Smoke:
                if (sub.smoke) {
                    const auto& sm = def.smoke;
                    auto& cb = *static_cast<SmokeParamsCB*>(sub.cbMapped);
                    cb.color         = tint4(sm.color, sub.tint);
                    cb.smokeColor    = tint4(sm.smokeColor, sub.tint);
                    cb.center        = sub.smokeCenter; // 上昇を反映（フレネル法線の中心と一致させる）
                    cb.radius        = sub.smokeRadius; // 膨張を反映（メッシュと一致）
                    cb.time          = previewTimer_;
                    cb.noiseScale    = sm.noiseScale;
                    cb.noiseStrength = sm.noiseStrength;
                    cb.scrollSpeed   = sm.scrollSpeed;
                    cb.fresnelPower  = sm.fresnelPower;
                    cb.density       = sm.density;
                    cb.noiseOctaves  = sm.noiseOctaves;
                    cb.rimIntensity  = sm.rimIntensity;
                    // Smoke の見た目はモーション駆動（色/フェード=CB color、膨張/上昇=center/radius）。
                    // シェーダは burst を使わないので常に -1（未使用）。
                    cb.burst         = -1.0f;

                    const auto& idx = pm->GetParameterIndices("VfxMeshSmoke");
                    cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshSmoke"));
                    cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshSmoke"));
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camAddr);
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                    sub.smoke->Draw(cmdList);
                }
                break;

            case VfxSubEffectType::Lightning:
                if (sub.lightning) {
                    const auto& lt = def.lightning;
                    auto& cb = *static_cast<LightningParamsCB*>(sub.cbMapped);
                    cb.color            = tint4(lt.color, sub.tint);
                    cb.glowColor        = tint4(lt.glowColor, sub.tint);
                    cb.branchColor      = tint4(lt.branchColor, sub.tint);
                    cb.time             = previewTimer_;
                    cb.glowPower        = lt.glowPower;
                    cb.coreWidth        = lt.coreWidth;
                    cb.solidness        = lt.solidness;
                    cb.outlineIntensity = lt.outlineIntensity;
                    cb._pad0 = cb._pad1 = cb._pad2 = 0.0f;

                    const auto& idx = pm->GetParameterIndices("VfxMeshLightning");
                    cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshLightning"));
                    cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshLightning"));
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camAddr);
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                    sub.lightning->Draw(cmdList);
                }
                break;

            case VfxSubEffectType::Shockwave:
                if (sub.shockwave) {
                    const auto& sw = def.shockwave;
                    auto& cb = *static_cast<ShockwaveParamsCB*>(sub.cbMapped);
                    cb.color     = tint4(sw.color, sub.tint);
                    cb.time      = previewTimer_;
                    cb.duration  = sw.duration;
                    cb.thickness = sw.thickness;
                    // ワンショット時は「現サイクル内の経過 / 衝撃波の duration」で 0→1 を1回だけ進める。
                    // これで寿命中に1回だけ膨張して消え、休止をはさんで次サイクルでまた1回鳴る。
                    // （継続=モーション確認ループ中のみ -1 でシェーダ側 frac ループに任せる）
                    cb.burst = burstMode_
                        ? std::min(oneShotLocal_ / std::max(sw.duration, 0.01f), 1.0f) : -1.0f;

                    const auto& idx = pm->GetParameterIndices("VfxMeshShockwave");
                    cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshShockwave"));
                    cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshShockwave"));
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"), camAddr);
                    cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                    sub.shockwave->Draw(cmdList);
                }
                break;
            }
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

    void VfxMeshEditor::DrawLightVolumeSection(VfxSubEffect& sub)
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& lv = sub.lightVolume;

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
        ImGui::Checkbox("OBB ワイヤーフレーム表示", &showVolumeDebug_);
        if (showVolumeDebug_) ImGui::TextColored(ImVec4(1, 1, 0, 1), "  > OBB ワイヤーフレーム ON");
    }

    // 既定モーション付与ヘルパの前方宣言（定義は DrawSubEffectsSection の直前）
    static void ApplyDefaultSubEffectMotions(VfxSubEffect& sub);

    void VfxMeshEditor::DrawSmokeSection(VfxSubEffect& sub)
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& sm = sub.smoke;

        ImGui::SeparatorText("カラー  (rgb>1 で Bloom / a=濃度)");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat4("火球色##sm", &sm.color.x, 0.02f, 0.0f, 10.0f, "%.2f");
            c |= ImGui::DragFloat4("煙色(爆発後)##smk", &sm.smokeColor.x, 0.01f, 0.0f, 4.0f, "%.2f");
            c |= ImGui::DragFloat("上昇速度(爆発後)##smrise", &sm.riseSpeed, 0.02f, 0.0f, 8.0f, "%.2f");
            if (c) CommitChange(b, "Smoke 色");
            ImGui::TextDisabled("爆発ワンショット時: 火球色→煙色へ遷移し、上昇しながら漂って消えます");
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

        ImGui::SeparatorText("動き（モーションで作る）");
        {
            ImGui::TextDisabled("  Smoke の膨張/上昇/フェードは下の「この形状のモーション」で作ります。\n"
                                "  下のボタンで定番の動き（膨張+上昇+フェード）をまとめて追加できます。");
            // 定番の動き（膨張 ScaleOverLife / 上昇 Rise / フェード FadeInOut）を Motion として追加。
            // これらはそのまま JSON に保存され、追加後に自由に編集できる。
            if (ImGui::Button("定番の動きをMotionで追加##smtomotion")) {
                VfxEffectAsset b2 = sel->asset;
                ApplyDefaultSubEffectMotions(sub); // 膨張/上昇/フェードを追加
                CommitChange(b2, "Smoke に定番モーション追加");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("膨張(ScaleOverLife)/上昇(Rise)/フェード(FadeInOut) をモーションとして追加します。\n追加後は下の「この形状のモーション」で自由に編集できます。");
        }
    }

    void VfxMeshEditor::DrawLightningSection(VfxSubEffect& sub)
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& lt = sub.lightning;

        ImGui::SeparatorText("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            const ImGuiColorEditFlags hdr = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;
            c |= ImGui::ColorEdit4("芯の色##lt",   &lt.color.x,       hdr);
            c |= ImGui::ColorEdit4("グロー色##lt", &lt.glowColor.x,   hdr);
            c |= ImGui::ColorEdit4("枝の色##lt",   &lt.branchColor.x, hdr);
            if (c) CommitChange(b, "Lightning 色");
            ImGui::TextDisabled("  芯→縁を2色でグラデ、枝は別色。rgb>1 で Bloom 発光");
        }

        ImGui::SeparatorText("実体感 / アウトライン");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("芯の太さ##ltcw",    &lt.coreWidth,        0.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("実体感(透明感↓)##lts", &lt.solidness,     0.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("アウトライン強調##lto", &lt.outlineIntensity, 0.0f, 4.0f, "%.2f");
            if (c) CommitChange(b, "Lightning 実体感");
            ImGui::TextDisabled("  実体感を上げると透け感が消える。アウトラインで枝が際立つ");
        }

        ImGui::SeparatorText("方向 / 曲線");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            // プリセット
            if (ImGui::SmallButton("縦##ltd")) { lt.direction = { 0,1,0 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("横##ltd")) { lt.direction = { 1,0,0 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("奥##ltd")) { lt.direction = { 0,0,1 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("斜め##ltd")) { lt.direction = { 1,1,0 }; c = true; }
            c |= ImGui::DragFloat3("方向(自由)##ltd", &lt.direction.x, 0.02f, -1.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("曲げ量(弧)##ltbend", &lt.bendAmount, -5.0f, 5.0f, "%.2f");
            if (c) CommitChange(b, "Lightning 方向/曲線");
            ImGui::TextDisabled("  方向で縦/横/奥/自由。曲げ量>0 で弓なりの弧になる");
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
    }

    void VfxMeshEditor::DrawShockwaveSection(VfxSubEffect& sub)
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& sw = sub.shockwave;

        ImGui::SeparatorText("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::DragFloat4("色##sw", &sw.color.x, 0.02f, 0.0f, 10.0f, "%.2f"))
                CommitChange(b, "Shockwave 色");
        }

        ImGui::SeparatorText("形状 / 速度");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat("最大半径##swr",  &sw.radius,    0.05f, 0.1f, 50.0f, "%.2f");
            c |= ImGui::DragFloat("膨張時間(秒)##swd", &sw.duration, 0.01f, 0.05f, 5.0f, "%.2f");
            c |= ImGui::SliderFloat("リング太さ##swt", &sw.thickness, 0.01f, 1.0f);
            if (c) CommitChange(b, "Shockwave パラメータ");
        }
    }

    // モーションリスト編集（エフェクト全体用と形状個別用で共用）
    void VfxMeshEditor::DrawMotionListUI(std::vector<VfxMotion>& motions,
                                         bool showTarget, const char* commitLabel)
    {
        auto* sel = Selected();
        if (!sel) return;

        const char* typeNames[] = {
            "BurstGrow (爆発の寿命)", "Move (等速移動)", "Rise (上昇)", "Pulse (脈動)",
            "ScaleOverLife (スケール変化)", "ColorOverLife (色変化)", "FadeInOut (フェード)",
            "Accelerate (加速/重力)", "Orbit (周回)", "Shake (揺れ)",
            "Visibility (表示期間)", "Flicker (明滅)"
        };
        const char* easeNames[] = {
            "Linear", "EaseIn (ゆっくり開始)", "EaseOut (ゆっくり終了)", "EaseInOut (両端)",
            "EaseOutCubic (強めブレーキ)", "EaseOutExpo (爆発向き)", "EaseOutBack (ポップ)"
        };
        const ImGuiColorEditFlags hdr = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(motions.size()); ++i) {
            ImGui::PushID(i);
            auto& m = motions[i];

            VfxEffectAsset b = sel->asset;
            bool c = false;

            int typeIdx = static_cast<int>(m.type);
            ImGui::SetNextItemWidth(220);
            if (ImGui::Combo("種類##mt", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
                m.type = static_cast<VfxMotionType>(typeIdx);
                c = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("削除##m")) removeIdx = i;

            // 適用対象（BurstGrow は効果全体の寿命なので対象指定なし。
            // 形状個別モーションはその形状にだけ効くので対象指定は不要）
            if (showTarget && m.type != VfxMotionType::BurstGrow) {
                const char* targetNames[] = { "All (全効果)", "Smoke", "Lightning", "Shockwave", "LightVolume" };
                int tgtIdx = static_cast<int>(m.target);
                ImGui::SetNextItemWidth(220);
                if (ImGui::Combo("対象##mtg", &tgtIdx, targetNames, IM_ARRAYSIZE(targetNames))) {
                    m.target = static_cast<VfxMotionTarget>(tgtIdx);
                    c = true;
                }
            }

            // タイミング（BurstGrow 以外の全タイプ共通）
            if (m.type != VfxMotionType::BurstGrow) {
                ImGui::SetNextItemWidth(90);
                c |= ImGui::DragFloat("開始遅延(秒)##mst", &m.startTime, 0.02f, 0.0f, 30.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90);
                c |= ImGui::DragFloat("効果時間(秒)##mwin", &m.window, 0.02f, 0.0f, 30.0f, "%.2f");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("0 = 無限（Scale/Color はワンショット寿命全体で 0→1）\n"
                                      ">0 = この秒数で完了（移動系はここで停止）");
            }

            // イージング（補間系のみ）
            if (m.type == VfxMotionType::ScaleOverLife || m.type == VfxMotionType::ColorOverLife) {
                int easeIdx = static_cast<int>(m.ease);
                ImGui::SetNextItemWidth(220);
                if (ImGui::Combo("カーブ##mease", &easeIdx, easeNames, IM_ARRAYSIZE(easeNames))) {
                    m.ease = static_cast<VfxEase>(easeIdx);
                    c = true;
                }
            }

            // type ごとに使うパラメータだけ表示
            switch (m.type) {
            case VfxMotionType::BurstGrow:
                c |= ImGui::DragFloat("寿命(秒)##md", &m.duration, 0.02f, 0.05f, 10.0f, "%.2f");
                ImGui::TextDisabled("  ※ワンショットの全体寿命。Smokeの膨張/上昇もこの時間で進む");
                break;
            case VfxMotionType::Move:
                c |= ImGui::DragFloat3("速度(/秒)##mv", &m.velocity.x, 0.02f, -20.0f, 20.0f, "%.2f");
                break;
            case VfxMotionType::Rise:
                c |= ImGui::DragFloat("上昇速度##mr", &m.velocity.y, 0.02f, -20.0f, 20.0f, "%.2f");
                c |= ImGui::DragFloat("係数##ma",     &m.amplitude, 0.02f, 0.0f, 10.0f, "%.2f");
                break;
            case VfxMotionType::Pulse:
                c |= ImGui::DragFloat("振幅##mp",      &m.amplitude, 0.01f, 0.0f, 2.0f, "%.2f");
                c |= ImGui::DragFloat("周波数(Hz)##mf", &m.frequency, 0.05f, 0.0f, 20.0f, "%.2f");
                break;
            case VfxMotionType::ScaleOverLife:
                c |= ImGui::DragFloat("開始スケール##mss", &m.scaleStart, 0.01f, 0.0f, 20.0f, "%.2f");
                c |= ImGui::DragFloat("終了スケール##mse", &m.scaleEnd,   0.01f, 0.0f, 20.0f, "%.2f");
                ImGui::TextDisabled("  ※元サイズへの倍率。0→1.5 で「破裂して広がる」");
                break;
            case VfxMotionType::ColorOverLife:
                c |= ImGui::ColorEdit4("開始色(乗算)##mcs", &m.colorStart.x, hdr);
                c |= ImGui::ColorEdit4("終了色(乗算)##mce", &m.colorEnd.x,   hdr);
                ImGui::TextDisabled("  ※元の色に掛ける倍率。白(1,1,1,1)=変化なし / >1 で Bloom 強化\n"
                                    "     火球→煙なら 開始(1,1,1,1) → 終了(0.2,0.2,0.25,0.9) など");
                break;
            case VfxMotionType::FadeInOut:
                c |= ImGui::DragFloat("フェードイン(秒)##mfi",  &m.fadeIn,  0.01f, 0.0f, 10.0f, "%.2f");
                c |= ImGui::DragFloat("フェードアウト(秒)##mfo", &m.fadeOut, 0.01f, 0.0f, 10.0f, "%.2f");
                ImGui::TextDisabled("  ※アウトは効果時間（0ならワンショット寿命）の終端から逆算");
                break;
            case VfxMotionType::Accelerate:
                c |= ImGui::DragFloat3("初速(/秒)##mav",   &m.velocity.x,     0.02f, -50.0f, 50.0f, "%.2f");
                c |= ImGui::DragFloat3("加速度(/秒²)##maa", &m.acceleration.x, 0.02f, -50.0f, 50.0f, "%.2f");
                ImGui::TextDisabled("  ※重力なら加速度 Y=-9.8。打ち上げは初速+Y & 加速度-Y");
                break;
            case VfxMotionType::Orbit:
                c |= ImGui::DragFloat3("回転軸##mox",     &m.velocity.x, 0.02f, -1.0f, 1.0f, "%.2f");
                c |= ImGui::DragFloat("半径##mor",        &m.amplitude, 0.02f, 0.0f, 20.0f, "%.2f");
                c |= ImGui::DragFloat("回転数(/秒)##mof", &m.frequency, 0.05f, -10.0f, 10.0f, "%.2f");
                break;
            case VfxMotionType::Shake:
                c |= ImGui::DragFloat("振れ幅##msa",     &m.amplitude, 0.01f, 0.0f, 5.0f, "%.2f");
                c |= ImGui::DragFloat("速さ(Hz)##msf",   &m.frequency, 0.1f,  0.0f, 60.0f, "%.1f");
                break;
            case VfxMotionType::Visibility:
                ImGui::TextDisabled("  ※開始遅延〜開始遅延+効果時間 の間だけ表示（効果時間0=以降ずっと表示）");
                ImGui::TextDisabled("     例: 閃光=0.00〜0.15秒 / リング=0.00〜0.50秒 と順序付けできる");
                break;
            case VfxMotionType::Flicker:
                c |= ImGui::DragFloat("明滅の深さ##mfa",   &m.amplitude, 0.01f, 0.0f, 1.0f, "%.2f");
                c |= ImGui::DragFloat("明滅回数(/秒)##mff", &m.frequency, 0.5f,  0.0f, 120.0f, "%.1f");
                break;
            }

            if (c) CommitChange(b, commitLabel);
            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            motions.erase(motions.begin() + removeIdx);
            CommitChange(b, "モーション削除");
        }

        if (ImGui::Button("＋ モーション追加")) {
            VfxEffectAsset b = sel->asset;
            motions.push_back(VfxMotion{});
            CommitChange(b, "モーション追加");
        }
        if (motions.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(動きなし = 従来挙動)");
        }
    }

    void VfxMeshEditor::DrawMotionSection()
    {
        auto* sel = Selected();
        if (!sel) return;

        ImGui::TextDisabled("エフェクト全体の動き（寿命・移動・脈動）をデータで定義。ゲームでも同じ動きで再生されます。");
        ImGui::TextDisabled("形状1個だけを動かしたい場合は、形状タブの各インスタンス内のモーションを使ってください。");
        ImGui::Spacing();

        DrawMotionListUI(sel->asset.motions, true, "モーション編集");
    }

    // -------------------------------------------------------
    // 形状（サブ効果）リスト: 同じ種類を複数追加・複製・削除できる
    // -------------------------------------------------------
    // 新規サブ効果を「ハードコードではなくモーションで動く」状態にするための既定モーション。
    // ここで積んだ VfxMotion はそのまま JSON に保存され、エディタで自由に編集できる。
    // （Smoke の膨張/上昇/フェードはシェーダや Drive に埋め込まず、必ずここで Motion として与える）
    static void ApplyDefaultSubEffectMotions(VfxSubEffect& sub)
    {
        switch (sub.type) {
        case VfxSubEffectType::Smoke: {
            // 従来のハードコード破裂（builtInBurstMotion）は使わず、動きは全部モーションで作る
            sub.smoke.builtInBurstMotion = false;
            sub.smoke.riseSpeed = 0.f; // 上昇も Rise モーションで
            // ベース色は白（中立）にしておく。ColorOverLife は色を「乗算」するので、
            // ベースに色が付いていると灰色にしても色が残る。白にすると ColorOverLife の色が
            // そのまま出て「暖色→灰色」の変化がちゃんと出る。
            sub.smoke.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            // 膨張: 一気に広がって減速（EaseOutExpo）
            VfxMotion grow;
            grow.type = VfxMotionType::ScaleOverLife;
            grow.ease = VfxEase::EaseOutExpo;
            grow.window = 2.0f;
            grow.scaleStart = 0.3f; grow.scaleEnd = 1.6f;
            sub.motions.push_back(grow);

            // 上昇: 0.2 秒後から浮力で上がる
            VfxMotion rise;
            rise.type = VfxMotionType::Rise;
            rise.startTime = 0.2f;
            rise.window = 1.8f;
            rise.velocity = { 0.f, 1.0f, 0.f };
            rise.amplitude = 1.0f;
            sub.motions.push_back(rise);

            // フェード（不透明度）: 立ち上がり一瞬 → 終盤 0.7 秒で消える
            VfxMotion fade;
            fade.type = VfxMotionType::FadeInOut;
            fade.window = 2.0f;
            fade.fadeIn = 0.05f; fade.fadeOut = 0.7f;
            sub.motions.push_back(fade);

            // 色変化: 暖色（火球）→ 灰色の煙へ（α は FadeInOut に任せるので 1 のまま）。
            // ＝「フェードみたいに色を変えていく」動き。ベース白 × この色 が実際の見た目になる。
            //  ・window を短め(0.7s)＋EaseOutで「灰色になるのを早く」＝まだ濃いうちに灰色が見える
            //    （window=寿命(2s)だと消えかけてから灰色になり、灰色が見えない）。
            //  ・colorStart は控えめ(r を少しだけ>1)にして白飛びを防ぐ（>1が大きいと Bloom で真っ白）。
            VfxMotion color;
            color.type = VfxMotionType::ColorOverLife;
            color.ease = VfxEase::EaseOutCubic;
            color.window = 0.7f;
            color.colorStart = { 1.2f, 0.6f, 0.3f, 1.0f };   // 立ち上がりは暖色（火球。控えめHDR）
            color.colorEnd   = { 0.22f, 0.22f, 0.24f, 1.0f }; // すぐ灰色の煙に落ち着く
            sub.motions.push_back(color);

            // billow: サイズをゆっくり脈動させて「もくもく」感を出す（Pulse=scale を sin で脈動）
            VfxMotion pulse;
            pulse.type = VfxMotionType::Pulse;
            pulse.amplitude = 0.08f; // 振幅 8%
            pulse.frequency = 1.2f;  // Hz
            sub.motions.push_back(pulse);

            // turbulence: 位置を細かく揺らして乱流っぽい漂いを足す（Shake=位置ジッタ）
            VfxMotion shake;
            shake.type = VfxMotionType::Shake;
            shake.amplitude = 0.06f;
            shake.frequency = 1.5f;
            sub.motions.push_back(shake);
            break;
        }
        default:
            // 他の形状は既定モーションなし（必要になったらここに追加する）
            break;
        }
    }

    void VfxMeshEditor::DrawSubEffectsSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& subs = sel->asset.subEffects;

        ImGui::TextDisabled("形状（Smoke/Lightning/Shockwave/LightVolume）を好きな数だけ追加できます。同じ種類の複数追加も可能。");
        ImGui::Spacing();

        // ── 追加ボタン ──
        if (ImGui::Button("＋ 形状を追加")) ImGui::OpenPopup("##addSubEffect");
        if (ImGui::BeginPopup("##addSubEffect")) {
            const VfxSubEffectType addTypes[] = {
                VfxSubEffectType::Smoke, VfxSubEffectType::Lightning,
                VfxSubEffectType::Shockwave, VfxSubEffectType::LightVolume,
            };
            for (VfxSubEffectType t : addTypes) {
                if (ImGui::MenuItem(VfxSubEffectTypeName(t))) {
                    VfxEffectAsset b = sel->asset;
                    VfxSubEffect sub;
                    sub.type = t;
                    // 生成時に既定の動きを Motion として積む（ハードコードに頼らず JSON 保存＆編集可能）
                    ApplyDefaultSubEffectMotions(sub);
                    subs.push_back(std::move(sub));
                    CommitChange(b, "形状追加");
                }
            }
            ImGui::EndPopup();
        }
        if (subs.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(形状なし)");
        }
        ImGui::Separator();

        // ── インスタンス一覧 ──
        int removeIdx = -1;
        int dupIdx    = -1;
        for (int i = 0; i < static_cast<int>(subs.size()); ++i) {
            ImGui::PushID(i);
            auto& sub = subs[i];

            // ヘッダ: [有効/無効アイコン] 種類 + ラベル
            std::string title = std::string(sub.enabled ? (ICON_FA_CHECK " ") : (ICON_FA_BAN " "))
                              + VfxSubEffectTypeName(sub.type);
            if (!sub.label.empty()) title += "  \"" + sub.label + "\"";
            title += "###subHeader";

            bool open = ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            if (open) {
                ImGui::Indent();

                // 有効化 / 複製 / 削除
                {
                    VfxEffectAsset b = sel->asset;
                    if (ImGui::Checkbox("有効##sub", &sub.enabled)) CommitChange(b, "形状 有効切替");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("複製##sub")) dupIdx = i;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("この形状をパラメータごとコピーして追加");
                ImGui::SameLine();
                if (ImGui::SmallButton("削除##sub")) removeIdx = i;

                // 表示名（エディタ整理用）
                {
                    char labelBuf[128] = {};
                    strncpy_s(labelBuf, sizeof(labelBuf), sub.label.c_str(), _TRUNCATE);
                    ImGui::SetNextItemWidth(200);
                    VfxEffectAsset b = sel->asset;
                    if (ImGui::InputText("表示名##sub", labelBuf, sizeof(labelBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        sub.label = labelBuf;
                        CommitChange(b, "形状 表示名");
                    }
                }

                // 配置オフセット
                {
                    VfxEffectAsset b = sel->asset;
                    if (ImGui::DragFloat3("オフセット##sub", &sub.offset.x, 0.05f, -50.f, 50.f, "%.2f"))
                        CommitChange(b, "形状 オフセット");
                    ImGui::TextDisabled("  ※エフェクト基準位置からのずらし量。同種を並べる時に使う");
                }

                ImGui::Spacing();

                // 種類ごとのパラメータ
                switch (sub.type) {
                case VfxSubEffectType::LightVolume: DrawLightVolumeSection(sub); break;
                case VfxSubEffectType::Smoke:       DrawSmokeSection(sub);       break;
                case VfxSubEffectType::Lightning:   DrawLightningSection(sub);   break;
                case VfxSubEffectType::Shockwave:   DrawShockwaveSection(sub);   break;
                }

                // この形状専用のモーション
                ImGui::SeparatorText("この形状のモーション");
                ImGui::TextDisabled("  ※エフェクト全体のモーション（Motionタブ）に加算で適用されます");
                DrawMotionListUI(sub.motions, false, "形状モーション編集");

                ImGui::Unindent();
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (dupIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            VfxSubEffect copy = subs[dupIdx];
            subs.insert(subs.begin() + dupIdx + 1, std::move(copy));
            CommitChange(b, "形状複製");
        }
        if (removeIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            subs.erase(subs.begin() + removeIdx);
            CommitChange(b, "形状削除");
        }
    }

    void VfxMeshEditor::DrawPreviewSection()
    {
        ImGui::SeparatorText("プレビュー");

        // 軌道アニメ / 剣の長さは Trail 専用 → Trail 有効時のみ表示
        if (auto* selT = Selected(); selT && selT->asset.useTrail) {
            const char* animNames[] = { "Wobble (往復)", "Slash (横なぎ)", "Slash (縦斬り)", "Spin (回転)" };
            int animIdx = static_cast<int>(previewAnim_);
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("軌道アニメ", &animIdx, animNames, IM_ARRAYSIZE(animNames))) {
                previewAnim_ = static_cast<PreviewAnimMode>(animIdx);
                if (previewTrailEmitter_) previewTrailEmitter_->Play(); // アニメ変更時にリセットして再生
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::DragFloat("剣のサイズ", &swordLength_, 0.1f, 0.5f, 10.f, "%.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::DragFloat("再生速度", &previewSpeed_, 0.05f, 0.1f, 5.f, "%.2f");

            if (ImGui::Checkbox("ワンショット再生 (1振りして消えるまで)", &trailOneShot_)) {
                previewTimer_ = 0.f;
                if (previewTrailEmitter_) previewTrailEmitter_->Play();
            }
        }

        if (previewPlaying_) {
            // 停止ボタン
            if (ImGui::Button((std::string(ICON_FA_STOP) + " 停止").c_str())) {
                previewPlaying_ = false;
                if (previewTrailEmitter_) previewTrailEmitter_->Stop(); // Emitterを停止（描画されなくなる）
            }
        }
        else {
            // 再生ボタン
            if (ImGui::Button((std::string(ICON_FA_PLAY) + " 再生").c_str())) {
                previewPlaying_ = true;
                previewTimer_ = 0.f;
                if (previewTrailEmitter_) previewTrailEmitter_->Play(); // Emitterを再生（描画再開）
            }
        }
        ImGui::SameLine();
        // リセットボタン
        if (ImGui::Button((std::string(ICON_FA_SYNC) + " リセット").c_str())) {
            previewTimer_ = 0.f;
            if (previewTrailEmitter_) previewTrailEmitter_->Play(); // リセットして最初から再生
        }

        // ループ再生：ON なら寿命ぶんで1回再生 → 休止 → まっさらに再Emit をくりかえす
        // （パーティクルのワンショットループ相当）。OFF なら1回だけ再生して止まる。
        if (ImGui::Checkbox("ループ再生 (1回ずつ繰り返しEmit)", &loopOneShot_)) {
            previewTimer_ = 0.f;
            if (previewTrailEmitter_) previewTrailEmitter_->Play();
        }
        ImGui::TextDisabled("  OFF=1回だけ再生して停止 / ON=1回ずつ繰り返し。動きはモーションで作る。");

        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("寿命フォールバック(s)", &burstDuration_, 0.02f, 0.1f, 5.0f, "%.2f");
        ImGui::SameLine();
        ImGui::TextDisabled("BurstGrow モーションがあればそちら優先");
        if (loopOneShot_) {
            ImGui::SetNextItemWidth(120);
            ImGui::DragFloat("休止(s)", &oneShotGap_, 0.02f, 0.0f, 5.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("次のEmitまでの間（完全に消える時間）");
        }
        if (ImGui::Button("もう一度")) previewTimer_ = 0.f;

        ImGui::DragFloat3("プレビュー位置", &previewCenter_.x, 0.05f);
        ImGui::SliderAngle("プレビューYaw", &previewYaw_, -180.f, 180.f);

        auto* sel = Selected();
        if (sel) {
            // ワンショット進捗バー（burstProgress_ は 0→1、ループの休止中は 1 で貼り付く）
            if (burstMode_) {
                char ov[48];
                snprintf(ov, sizeof(ov), "%s  %.0f%%",
                         loopOneShot_ ? "ループEmit" : "ワンショット",
                         std::max(burstProgress_, 0.f) * 100.f);
                ImGui::ProgressBar(std::max(burstProgress_, 0.f), ImVec2(-1, 0), ov);
            } else if (sel->asset.useTrail && sel->asset.trail.lifetime > 0.f) {
                char ov[48];
                snprintf(ov, sizeof(ov), "Trail  %.2f / %.2f s",
                    previewTimer_, sel->asset.trail.lifetime);
                ImGui::ProgressBar(
                    std::min(previewTimer_ / sel->asset.trail.lifetime, 1.f),
                    ImVec2(-1, 0), ov);
            }
            for (const auto& sub : sel->asset.subEffects) {
                if (sub.type == VfxSubEffectType::LightVolume && sub.enabled) {
                    ImGui::ProgressBar(1.f, ImVec2(-1, 0), "Light Volume  (active)");
                    break;
                }
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

    void VfxMeshEditor::RenameCurrentFile(const std::string& newName)
    {
        auto* sel = Selected();
        if (!sel) return;

        // 空名 / 不正文字はスキップ（ファイル名として使えない文字が入っていたら何もしない）
        if (newName.empty()) return;
        if (newName.find_first_of("\\/:*?\"<>|") != std::string::npos) {
            Logger("VfxMeshEditor: ファイル名に使えない文字が含まれるためリネームしません -> " + newName);
            return;
        }

        fs::path oldPath(sel->filePath);
        fs::path dir = oldPath.parent_path();
        if (dir.empty()) dir = fs::path(scanRoot_);

        // 既に一致しているなら何もしない（拡張子を除いたファイル名で比較）
        if (oldPath.stem().string() == newName) return;

        // 他エントリや既存ファイルと衝突しないパスを探す（末尾に番号を付ける）
        auto conflicts = [&](const fs::path& p) {
            std::error_code e;
            if (fs::exists(p, e)) return true;
            for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
                if (i == selectedIndex_) continue;
                if (fs::path(entries_[i].filePath) == p) return true;
            }
            return false;
        };
        fs::path newPath = dir / (newName + ".json");
        for (int n = 1; conflicts(newPath); ++n) {
            newPath = dir / (newName + std::to_string(n) + ".json");
        }

        // ディスク上に旧ファイルがあれば実際にリネーム。無ければ（未保存の新規）パス変更のみ。
        std::error_code ec;
        if (fs::exists(oldPath, ec)) {
            fs::rename(oldPath, newPath, ec);
            if (ec) {
                Logger("VfxMeshEditor: リネーム失敗 " + oldPath.string() + " -> " + newPath.string());
                return;
            }
            Logger("VfxMeshEditor: リネーム " + oldPath.string() + " -> " + newPath.string());
        }

        sel->filePath = newPath.string();
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

    // 編集を即時プレビューに反映
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
                    if (previewTrailEmitter_) previewTrailEmitter_->SetAsset(after); // 即時反映
                }
            },
            [this, idx, before]() {
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = before;
                    entries_[idx].isDirty = true;
                    if (previewTrailEmitter_) previewTrailEmitter_->SetAsset(before); // Undo即時反映
                }
            }
        ));

        sel->isDirty = true;

        // 今回の編集分もプレビューに反映させる
        if (previewTrailEmitter_) {
            previewTrailEmitter_->SetAsset(sel->asset);
        }
    }

    // アセットの subEffects と previewSubs_ の構成（数と種類）を同期させる。
    // 一致していれば何もしない。変わっていたら全部作り直す（編集時のみなので軽い）。
    void VfxMeshEditor::SyncPreviewSubs()
    {
        auto* sel = Selected();
        if (!sel) { previewSubs_.clear(); return; }
        const auto& subs = sel->asset.subEffects;

        bool match = (previewSubs_.size() == subs.size());
        if (match) {
            for (size_t i = 0; i < subs.size(); ++i) {
                if (previewSubs_[i]->type != subs[i].type) { match = false; break; }
            }
        }
        if (match) return;

        previewSubs_.clear();
        previewSubs_.reserve(subs.size());

        for (const auto& def : subs) {
            auto sub = std::make_unique<PreviewSub>();
            sub->type = def.type;

            switch (def.type) {
            case VfxSubEffectType::LightVolume:
                sub->volume = std::make_unique<LightVolumeMesh>();
                sub->volume->Initialize(def.lightVolume);
                sub->cbRes = dxCommon_->CreateBufferResource(AlignedSize<LightVolumeParamsCB>());
                break;
            case VfxSubEffectType::Smoke:
                sub->smoke = std::make_unique<VolumeSmokeMesh>();
                sub->smoke->Initialize();
                sub->cbRes = dxCommon_->CreateBufferResource(AlignedSize<SmokeParamsCB>());
                break;
            case VfxSubEffectType::Lightning:
                sub->lightning = std::make_unique<LightningMesh>();
                sub->lightning->Initialize();
                sub->cbRes = dxCommon_->CreateBufferResource(AlignedSize<LightningParamsCB>());
                break;
            case VfxSubEffectType::Shockwave:
                sub->shockwave = std::make_unique<ShockwaveMesh>();
                sub->shockwave->Initialize();
                sub->cbRes = dxCommon_->CreateBufferResource(AlignedSize<ShockwaveParamsCB>());
                break;
            }
            sub->cbRes->Map(0, nullptr, &sub->cbMapped);

            previewSubs_.push_back(std::move(sub));
        }
    }

    VfxEffectAsset VfxMeshEditor::MakePreset(VfxPreset preset)
    {
        VfxEffectAsset a;

        // LightVolume のサブ効果を1つ足すヘルパ
        auto addVolume = [&a](const Vector3& halfExtents, const Vector4& color, float intensity) {
            VfxSubEffect sub;
            sub.type = VfxSubEffectType::LightVolume;
            sub.lightVolume.halfExtents = halfExtents;
            sub.lightVolume.color       = color;
            sub.lightVolume.intensity   = intensity;
            a.subEffects.push_back(std::move(sub));
        };

        switch (preset) {
        case VfxPreset::TrailOnly:
            a.useTrail = true; break;
        case VfxPreset::VolumeOnly:
            a.useTrail = false;
            addVolume({ 2.f, 1.5f, 5.f }, { 1.f, 0.9f, 0.f, 0.15f }, 1.0f);
            break;
        case VfxPreset::Sword:
            a.useTrail = true;
            a.trail.widthStart = 0.05f; a.trail.widthEnd = 0.f;
            a.trail.lifetime = 0.25f; a.trail.maxPoints = 48;
            a.trail.colorStart = { 1.f, 0.95f, 0.7f, 1.f };
            a.trail.colorEnd = { 0.8f, 0.5f, 0.1f, 0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.3f;
            addVolume({ 0.04f, 0.04f, 0.6f }, { 1.f, 0.9f, 0.5f, 0.18f }, 1.5f);
            break;
        case VfxPreset::Magic:
            a.useTrail = true;
            a.trail.widthStart = 0.4f;  a.trail.widthEnd = 0.05f;
            a.trail.lifetime = 1.2f;  a.trail.maxPoints = 96;
            a.trail.colorStart = { 0.4f, 0.2f, 1.f, 1.f };
            a.trail.colorEnd = { 0.2f, 0.6f, 1.f, 0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.8f;
            addVolume({ 1.5f, 1.5f, 4.f }, { 0.5f, 0.3f, 1.f, 0.25f }, 3.f);
            break;

        // 爆発ワンショット機能に頼らず、モーションの組み合わせだけで作った爆発。
        // 「破裂ポップ → 火球が煙に変色しつつ上昇 → フェード消滅」＋「衝撃波リング」＋「閃光」
        case VfxPreset::Explosion: {
            a.useTrail = false;

            // 全体寿命 2.2 秒（ゲームのワンショット再生・プレビューのリピートに使われる）
            {
                VfxMotion life;
                life.type = VfxMotionType::BurstGrow;
                life.duration = 2.2f;
                a.motions.push_back(life);
            }

            // ── 火球 → 煙 ──
            {
                VfxSubEffect fire;
                fire.type  = VfxSubEffectType::Smoke;
                fire.label = "火球";
                fire.smoke.color        = { 4.0f, 1.8f, 0.6f, 1.0f };   // HDR オレンジ（Bloom）
                fire.smoke.radius       = 1.5f;
                fire.smoke.riseSpeed    = 0.f;                          // 上昇はモーションで
                fire.smoke.noiseStrength = 0.9f;
                fire.smoke.rimIntensity = 3.0f;
                fire.smoke.builtInBurstMotion = false;                  // 従来破裂OFF＝モーション駆動

                VfxMotion pop;   // 破裂の急膨張（EaseOutExpo で一気に広がって減速）
                pop.type = VfxMotionType::ScaleOverLife;
                pop.ease = VfxEase::EaseOutExpo;
                pop.window = 0.5f;
                pop.scaleStart = 0.15f; pop.scaleEnd = 1.5f;
                fire.motions.push_back(pop);

                VfxMotion color; // 火球色 → 暗い煙色（後半で一気に暗く）
                color.type = VfxMotionType::ColorOverLife;
                color.ease = VfxEase::EaseInQuad;
                color.window = 2.2f;
                color.colorStart = { 1.0f, 1.0f, 1.0f, 1.0f };
                color.colorEnd   = { 0.05f, 0.05f, 0.06f, 0.85f };
                fire.motions.push_back(color);

                VfxMotion rise;  // 0.4 秒後から浮力で上昇
                rise.type = VfxMotionType::Rise;
                rise.startTime = 0.4f;
                rise.window = 1.8f;
                rise.velocity = { 0.f, 1.0f, 0.f };
                rise.amplitude = 1.0f;
                fire.motions.push_back(rise);

                VfxMotion fade;  // 立ち上がりは一瞬、終端 0.7 秒かけて消える
                fade.type = VfxMotionType::FadeInOut;
                fade.window = 2.2f;
                fade.fadeIn = 0.03f; fade.fadeOut = 0.7f;
                fire.motions.push_back(fade);

                a.subEffects.push_back(std::move(fire));
            }

            // ── 衝撃波リング（最初の 0.5 秒だけ） ──
            {
                VfxSubEffect ring;
                ring.type  = VfxSubEffectType::Shockwave;
                ring.label = "衝撃波";
                ring.shockwave.color     = { 6.0f, 2.5f, 1.0f, 1.0f }; // HDR 暖色
                ring.shockwave.radius    = 3.5f;
                ring.shockwave.duration  = 0.5f;
                ring.shockwave.thickness = 0.18f;

                VfxMotion show;
                show.type = VfxMotionType::Visibility;
                show.window = 0.5f;
                ring.motions.push_back(show);

                VfxMotion fade;
                fade.type = VfxMotionType::FadeInOut;
                fade.window = 0.5f;
                fade.fadeIn = 0.f; fade.fadeOut = 0.25f;
                ring.motions.push_back(fade);

                a.subEffects.push_back(std::move(ring));
            }

            // ── 閃光（最初の 0.18 秒だけ明滅） ──
            {
                VfxSubEffect flash;
                flash.type  = VfxSubEffectType::Lightning;
                flash.label = "閃光";
                flash.lightning.color       = { 8.0f, 8.0f, 10.0f, 1.0f }; // 白熱 HDR
                flash.lightning.glowColor   = { 6.0f, 6.0f, 9.0f, 1.0f };
                flash.lightning.branchColor = { 6.0f, 6.0f, 9.0f, 1.0f };
                flash.lightning.length   = 3.0f;
                flash.lightning.width    = 0.35f;
                flash.lightning.branches = 6;
                flash.lightning.flickerRate = 40.0f;

                VfxMotion show;
                show.type = VfxMotionType::Visibility;
                show.window = 0.18f;
                flash.motions.push_back(show);

                VfxMotion flicker;
                flicker.type = VfxMotionType::Flicker;
                flicker.amplitude = 0.5f;
                flicker.frequency = 60.0f;
                flash.motions.push_back(flicker);

                a.subEffects.push_back(std::move(flash));
            }
            break;
        }

        default: break;
        }
        return a;
    }

} // namespace YoRigine
#endif