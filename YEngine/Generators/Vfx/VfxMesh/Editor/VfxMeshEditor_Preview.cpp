#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include "DirectXCommon.h"
#include <PipelineManager/YPipelineManager.h>
#include <IconsFontAwesome5.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
    static constexpr size_t kCBVAlignment = 256;
    template<typename T>
    static constexpr size_t AlignedSize() {
        return (sizeof(T) + kCBVAlignment - 1) & ~(kCBVAlignment - 1);
    }
}

namespace YoRigine {

    void VfxMeshEditor::Update(float deltaTime)
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        previewTimer_ += deltaTime;
        const auto& asset = sel->asset;

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

        burstMode_ = (burstPeriod > 0.01f);
        oneShotLocal_ = 0.f;
        bool oneShotIdle = false;
        if (burstMode_) {
            if (loopOneShot_) {
                const float gap = std::max(oneShotGap_, 0.0f);
                const float cycle = burstPeriod + gap;
                oneShotLocal_ = std::fmod(previewTimer_, cycle);
                if (oneShotLocal_ >= burstPeriod) {
                    oneShotLocal_ = burstPeriod;
                    oneShotIdle = true;
                }
            } else {
                oneShotLocal_ = std::min(previewTimer_, burstPeriod);
            }
            burstProgress_ = std::min(oneShotLocal_ / burstPeriod, 1.0f);
        } else {
            burstProgress_ = -1.0f;
        }
        const float oneShotLocal = oneShotLocal_;

        SyncPreviewSubs();

        VfxEvalState mbase;
        mbase.age = burstMode_ ? oneShotLocal : previewTimer_;
        mbase.progress = burstProgress_;
        mbase.lifetime = burstMode_ ? burstPeriod : -1.f;
        mbase.position = previewCenter_;
        mbase.scale = 1.0f;

        if (asset.useTrail && previewTrailEmitter_) {
            Vector3 tip = previewCenter_;
            Vector3 root = previewCenter_;
            Vector3 widthDir = { 0.f, 1.f, 0.f };
            const float period = std::max(asset.trail.lifetime * 1.5f, 0.5f);

            bool addThisFrame = true;
            float t;
            if (trailOneShot_) {
                const float swingDur = period / std::max(previewSpeed_, 0.01f);
                const float cycle = swingDur + asset.trail.lifetime + 0.4f;
                const float local = std::fmod(previewTimer_, cycle);
                if (local < deltaTime && previewTrailEmitter_) previewTrailEmitter_->Play();
                if (local <= swingDur) {
                    t = std::min(local / swingDur, 1.0f);
                } else {
                    t = 1.0f;
                    addThisFrame = false;
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

            if (addThisFrame) previewTrailEmitter_->AddPoint(tip, root, widthDir);
            previewTrailEmitter_->Update(deltaTime);
        }

        const size_t subCount = std::min(asset.subEffects.size(), previewSubs_.size());
        for (size_t i = 0; i < subCount; ++i) {
            const auto& def = asset.subEffects[i];
            auto& sub = *previewSubs_[i];
            if (!def.enabled) continue;

            if (oneShotIdle) {
                sub.visible = false;
                continue;
            }

            VfxEvalState s = mbase;
            s.position += def.offset;
            EvaluateSubEffectMotions(asset, def, s);

            sub.tint = s.colorTint;
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
                    sub.smoke->ApplyParam(def.smoke);
                    sub.smoke->Drive(s);
                    sub.smoke->Update(deltaTime);
                    sub.smokeCenter = sub.smoke->GetCenter();
                    sub.smokeRadius = sub.smoke->GetRadius();
                }
                break;
            case VfxSubEffectType::Lightning:
                if (sub.lightning) {
                    sub.lightning->SetCamera(camera_);
                    sub.lightning->ApplyParam(def.lightning);
                    float half = def.lightning.length * 0.5f;
                    Vector3 dir = def.lightning.direction;
                    float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                    dir = (dl < 1e-4f) ? Vector3{ 0, 1, 0 } : Vector3{ dir.x / dl, dir.y / dl, dir.z / dl };
                    sub.lightning->SetEndpoints(s.position - dir * half, s.position + dir * half);
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

    void VfxMeshEditor::DrawPreview()
    {
        auto* sel = Selected();
        if (!sel || !previewPlaying_) return;

        const auto& asset = sel->asset;
        if (asset.useTrail && previewTrailEmitter_) {
            previewTrailEmitter_->SetCamera(camera_);
            previewTrailEmitter_->Draw();
        }
        if (!camera_) return;

        auto* pm = YPipelineManager::GetInstance();
        auto* cmdList = DirectXCommon::GetInstance()->GetCommandList().Get();
        D3D12_GPU_VIRTUAL_ADDRESS camAddr = camera_->GetCameraResource()->GetGPUVirtualAddress();

        auto tint4 = [](const Vector4& c, const Vector4& t) -> Vector4 {
            return { c.x * t.x, c.y * t.y, c.z * t.z, c.w * t.w };
        };

        const size_t subCount = std::min(asset.subEffects.size(), previewSubs_.size());
        for (size_t i = 0; i < subCount; ++i) {
            const auto& def = asset.subEffects[i];
            auto& sub = *previewSubs_[i];
            if (!def.enabled || !sub.cbMapped || !sub.cbRes) continue;
            if (!sub.visible || sub.tint.w <= 0.001f) continue;

            switch (def.type) {
            case VfxSubEffectType::LightVolume:
                if (sub.volume) {
                    const auto& lv = def.lightVolume;
                    auto& cb = *static_cast<LightVolumeParamsCB*>(sub.cbMapped);
                    cb.color[0] = lv.color.x * sub.tint.x;
                    cb.color[1] = lv.color.y * sub.tint.y;
                    cb.color[2] = lv.color.z * sub.tint.z;
                    cb.color[3] = lv.color.w * lv.intensity * sub.tint.w;
                    cb.edgeFade = lv.edgeFade;
                    cb.depthFade = lv.depthFade;
                    cb.noiseTiling = lv.noiseTiling;
                    cb.noiseStrength = lv.noiseStrength;
                    cb.time = previewTimer_;
                    cb.beamStrength = lv.beamStrength;
                    cb.beamRadius = lv.beamRadius;
                    cb.beamPower = lv.beamPower;
                    cb.beamGlow = lv.beamGlow;

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
                    cb.color = tint4(sm.color, sub.tint);
                    cb.smokeColor = tint4(sm.smokeColor, sub.tint);
                    cb.center = sub.smokeCenter;
                    cb.radius = sub.smokeRadius;
                    cb.time = previewTimer_;
                    cb.noiseScale = sm.noiseScale;
                    cb.noiseStrength = sm.noiseStrength;
                    cb.scrollSpeed = sm.scrollSpeed;
                    cb.fresnelPower = sm.fresnelPower;
                    cb.density = sm.density;
                    cb.noiseOctaves = sm.noiseOctaves;
                    cb.rimIntensity = sm.rimIntensity;
                    cb.burst = -1.0f;

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
                    cb.color = tint4(lt.color, sub.tint);
                    cb.glowColor = tint4(lt.glowColor, sub.tint);
                    cb.branchColor = tint4(lt.branchColor, sub.tint);
                    cb.time = previewTimer_;
                    cb.glowPower = lt.glowPower;
                    cb.coreWidth = lt.coreWidth;
                    cb.solidness = lt.solidness;
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
                    cb.color = tint4(sw.color, sub.tint);
                    cb.time = previewTimer_;
                    cb.duration = sw.duration;
                    cb.thickness = sw.thickness;
                    cb.burst = burstMode_
                        ? std::min(oneShotLocal_ / std::max(sw.duration, 0.01f), 1.0f)
                        : -1.0f;

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

    void VfxMeshEditor::DrawPreviewSection()
    {
        ImGui::SeparatorText("プレビュー");

        if (auto* selT = Selected(); selT && selT->asset.useTrail) {
            const char* animNames[] = { "Wobble (往復)", "Slash (横なぎ)", "Slash (縦斬り)", "Spin (回転)" };
            int animIdx = static_cast<int>(previewAnim_);
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("軌道アニメ", &animIdx, animNames, IM_ARRAYSIZE(animNames))) {
                previewAnim_ = static_cast<PreviewAnimMode>(animIdx);
                if (previewTrailEmitter_) previewTrailEmitter_->Play();
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
            if (ImGui::Button((std::string(ICON_FA_STOP) + " 停止").c_str())) {
                previewPlaying_ = false;
                if (previewTrailEmitter_) previewTrailEmitter_->Stop();
            }
        } else {
            if (ImGui::Button((std::string(ICON_FA_PLAY) + " 再生").c_str())) {
                previewPlaying_ = true;
                previewTimer_ = 0.f;
                if (previewTrailEmitter_) previewTrailEmitter_->Play();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string(ICON_FA_SYNC) + " リセット").c_str())) {
            previewTimer_ = 0.f;
            if (previewTrailEmitter_) previewTrailEmitter_->Play();
        }

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
            if (burstMode_) {
                char ov[48];
                std::snprintf(ov, sizeof(ov), "%s  %.0f%%",
                              loopOneShot_ ? "ループEmit" : "ワンショット",
                              std::max(burstProgress_, 0.f) * 100.f);
                ImGui::ProgressBar(std::max(burstProgress_, 0.f), ImVec2(-1, 0), ov);
            } else if (sel->asset.useTrail && sel->asset.trail.lifetime > 0.f) {
                char ov[48];
                std::snprintf(ov, sizeof(ov), "Trail  %.2f / %.2f s",
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

} // namespace YoRigine
#endif
