#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <cstring>
#include <string>
#include <utility>

namespace YoRigine {

    void VfxMeshEditor::DrawLightVolumeSection(VfxElement& sub)
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

        ImGui::SeparatorText("フェード / ノイズ");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("エッジフェード##lvedge", &lv.edgeFade, 0.001f, 0.5f, "%.3f");
            c |= ImGui::DragFloat("近接フェード距離##lvdepth", &lv.depthFade, 0.05f, 0.001f, 50.0f, "%.2f");
            c |= ImGui::DragFloat("ノイズ細かさ##lvnoiseTile", &lv.noiseTiling, 0.05f, 0.01f, 50.0f, "%.2f");
            c |= ImGui::SliderFloat("ノイズ強度##lvnoiseStr", &lv.noiseStrength, 0.0f, 1.0f, "%.2f");
            if (c) CommitChange(b, "Volume フェード/ノイズ");
        }

        ImGui::SeparatorText("ビーム");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("ビーム強度##lvbeamStr", &lv.beamStrength, 0.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("ビーム半径##lvbeamRadius", &lv.beamRadius, 0.01f, 1.5f, "%.2f");
            c |= ImGui::DragFloat("ビーム鋭さ##lvbeamPower", &lv.beamPower, 0.05f, 0.1f, 12.0f, "%.2f");
            c |= ImGui::DragFloat("ビーム発光##lvbeamGlow", &lv.beamGlow, 0.05f, 0.0f, 10.0f, "%.2f");
            if (c) CommitChange(b, "Volume ビーム");
        }

        ImGui::Spacing();
        ImGui::Checkbox("OBB ワイヤーフレーム表示", &showVolumeDebug_);
        if (showVolumeDebug_) ImGui::TextColored(ImVec4(1, 1, 0, 1), "  > OBB ワイヤーフレーム ON");
    }

    void VfxMeshEditor::DrawNoiseVolumeSection(VfxElement& sub)
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
            if (c) CommitChange(b, "NoiseVolume 色");
            ImGui::TextDisabled("爆発ワンショット時: 火球色→煙色へ遷移し、上昇しながら漂って消えます");
        }

        ImGui::SeparatorText("ボリューム / 渦巻き");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat("半径##smr", &sm.radius, 0.05f, 0.1f, 20.0f, "%.2f");
            c |= ImGui::DragFloat("ノイズスケール##sms", &sm.noiseScale, 0.05f, 0.1f, 16.0f, "%.2f");
            c |= ImGui::SliderFloat("渦巻きの強さ##smn", &sm.noiseStrength, 0.0f, 1.0f);
            c |= ImGui::DragFloat("スクロール速度##smc", &sm.scrollSpeed, 0.01f, 0.0f, 3.0f, "%.2f");
            c |= ImGui::DragFloat("縁の柔らかさ##smf", &sm.fresnelPower, 0.05f, 0.1f, 8.0f, "%.2f");
            c |= ImGui::DragFloat("密度##smd", &sm.density, 0.01f, 0.0f, 3.0f, "%.2f");
            c |= ImGui::DragFloat("オクターブ##smo", &sm.noiseOctaves, 0.1f, 1.0f, 5.0f, "%.1f");
            c |= ImGui::DragFloat("リム発光(フレア)##smrim", &sm.rimIntensity, 0.05f, 0.0f, 10.0f, "%.2f");
            if (c) CommitChange(b, "NoiseVolume パラメータ");
        }

        ImGui::SeparatorText("動き（モーションで作る）");
        {
            ImGui::TextDisabled("  NoiseVolume の膨張/上昇/フェードは下の「このエレメントのモーション」で作ります。");
            if (ImGui::Button("定番の動きをMotionで追加##smtomotion")) {
                VfxEffectAsset b2 = sel->asset;
                ApplyDefaultElementMotions(sub);
                CommitChange(b2, "NoiseVolume に定番モーション追加");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("膨張/上昇/フェード/色変化/揺れをモーションとして追加します。");
        }
    }

    void VfxMeshEditor::DrawLightningBoltSection(VfxElement& sub)
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& lt = sub.lightning;
        const ImGuiColorEditFlags hdr = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

        ImGui::SeparatorText("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::ColorEdit4("芯の色##lt", &lt.color.x, hdr);
            c |= ImGui::ColorEdit4("グロー色##lt", &lt.glowColor.x, hdr);
            c |= ImGui::ColorEdit4("枝の色##lt", &lt.branchColor.x, hdr);
            if (c) CommitChange(b, "LightningBolt 色");
        }

        ImGui::SeparatorText("実体感 / アウトライン");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::SliderFloat("芯の太さ##ltcw", &lt.coreWidth, 0.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("実体感(透明感↓)##lts", &lt.solidness, 0.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("アウトライン強調##lto", &lt.outlineIntensity, 0.0f, 4.0f, "%.2f");
            if (c) CommitChange(b, "LightningBolt 実体感");
        }

        ImGui::SeparatorText("方向 / 曲線");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            if (ImGui::SmallButton("縦##ltd")) { lt.direction = { 0,1,0 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("横##ltd")) { lt.direction = { 1,0,0 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("奥##ltd")) { lt.direction = { 0,0,1 }; c = true; } ImGui::SameLine();
            if (ImGui::SmallButton("斜め##ltd")) { lt.direction = { 1,1,0 }; c = true; }
            c |= ImGui::DragFloat3("方向(自由)##ltd", &lt.direction.x, 0.02f, -1.0f, 1.0f, "%.2f");
            c |= ImGui::SliderFloat("曲げ量(弧)##ltbend", &lt.bendAmount, -5.0f, 5.0f, "%.2f");
            if (c) CommitChange(b, "LightningBolt 方向/曲線");
        }

        ImGui::SeparatorText("稲妻 / 明滅");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat("長さ##ltlen", &lt.length, 0.05f, 0.1f, 50.0f, "%.2f");
            c |= ImGui::DragFloat("幅##ltw", &lt.width, 0.005f, 0.01f, 2.0f, "%.3f");
            c |= ImGui::DragFloat("ジグザグ振れ##ltj", &lt.jitter, 0.02f, 0.0f, 5.0f, "%.2f");
            c |= ImGui::SliderInt("分割数##lts", &lt.segments, 4, 64);
            c |= ImGui::SliderInt("枝の数##ltb", &lt.branches, 0, 8);
            c |= ImGui::DragFloat("枝の振れ##ltbj", &lt.branchJitter, 0.02f, 0.0f, 5.0f, "%.2f");
            c |= ImGui::DragFloat("明滅レート(回/秒)##ltf", &lt.flickerRate, 0.5f, 0.0f, 60.0f, "%.1f");
            c |= ImGui::DragFloat("芯のグロー##ltg", &lt.glowPower, 0.05f, 0.1f, 8.0f, "%.2f");
            if (c) CommitChange(b, "LightningBolt パラメータ");
        }
    }

    void VfxMeshEditor::DrawShockwaveRingSection(VfxElement& sub)
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& sw = sub.shockwave;

        ImGui::SeparatorText("カラー (rgb>1 で Bloom)");
        {
            VfxEffectAsset b = sel->asset;
            if (ImGui::DragFloat4("色##sw", &sw.color.x, 0.02f, 0.0f, 10.0f, "%.2f"))
                CommitChange(b, "ShockwaveRing 色");
        }

        ImGui::SeparatorText("リング / 速度");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= ImGui::DragFloat("最大半径##swr", &sw.radius, 0.05f, 0.1f, 50.0f, "%.2f");
            c |= ImGui::DragFloat("膨張時間(秒)##swd", &sw.duration, 0.01f, 0.05f, 5.0f, "%.2f");
            c |= ImGui::SliderFloat("リング太さ##swt", &sw.thickness, 0.01f, 1.0f);
            if (c) CommitChange(b, "ShockwaveRing パラメータ");
        }
    }

    void VfxMeshEditor::DrawElementsSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& subs = sel->asset.elements;

        ImGui::TextDisabled("エレメント（NoiseVolume/LightningBolt/ShockwaveRing/LightVolume）を好きな数だけ追加できます。同じ種類の複数追加も可能。");
        ImGui::Spacing();

        if (ImGui::Button("＋ エレメントを追加")) ImGui::OpenPopup("##addElement");
        if (ImGui::BeginPopup("##addElement")) {
            const VfxElementType addTypes[] = {
                VfxElementType::NoiseVolume, VfxElementType::LightningBolt,
                VfxElementType::ShockwaveRing, VfxElementType::LightVolume,
            };
            for (VfxElementType t : addTypes) {
                if (ImGui::MenuItem(VfxElementTypeName(t))) {
                    VfxEffectAsset b = sel->asset;
                    VfxElement sub;
                    sub.type = t;
                    ApplyDefaultElementMotions(sub);
                    subs.push_back(std::move(sub));
                    CommitChange(b, "エレメント追加");
                }
            }
            ImGui::EndPopup();
        }
        if (subs.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(エレメントなし)");
        }
        ImGui::Separator();

        int removeIdx = -1;
        int dupIdx = -1;
        for (int i = 0; i < static_cast<int>(subs.size()); ++i) {
            ImGui::PushID(i);
            auto& sub = subs[i];

            std::string title = std::string(sub.enabled ? (ICON_FA_CHECK " ") : (ICON_FA_BAN " "))
                + VfxElementTypeName(sub.type);
            if (!sub.label.empty()) title += "  \"" + sub.label + "\"";
            title += "###subHeader";

            bool open = ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (open) {
                ImGui::Indent();

                {
                    VfxEffectAsset b = sel->asset;
                    if (ImGui::Checkbox("有効##sub", &sub.enabled)) CommitChange(b, "エレメント 有効切替");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("複製##sub")) dupIdx = i;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("このエレメントをパラメータごとコピーして追加");
                ImGui::SameLine();
                if (ImGui::SmallButton("削除##sub")) removeIdx = i;

                {
                    char labelBuf[128] = {};
                    strncpy_s(labelBuf, sizeof(labelBuf), sub.label.c_str(), _TRUNCATE);
                    ImGui::SetNextItemWidth(200);
                    VfxEffectAsset b = sel->asset;
                    if (ImGui::InputText("表示名##sub", labelBuf, sizeof(labelBuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        sub.label = labelBuf;
                        CommitChange(b, "エレメント 表示名");
                    }
                }

                {
                    VfxEffectAsset b = sel->asset;
                    if (ImGui::DragFloat3("オフセット##sub", &sub.offset.x, 0.05f, -50.f, 50.f, "%.2f"))
                        CommitChange(b, "エレメント オフセット");
                }

                ImGui::Spacing();
                switch (sub.type) {
                case VfxElementType::LightVolume: DrawLightVolumeSection(sub); break;
                case VfxElementType::NoiseVolume:       DrawNoiseVolumeSection(sub);       break;
                case VfxElementType::LightningBolt:   DrawLightningBoltSection(sub);   break;
                case VfxElementType::ShockwaveRing:   DrawShockwaveRingSection(sub);   break;
                }

                ImGui::SeparatorText("このエレメントのモーション");
                ImGui::TextDisabled("  ※エフェクト全体のモーション（Motionタブ）に加算で適用されます");
                DrawMotionListUI(sub.motions, false, "エレメントモーション編集");

                ImGui::Unindent();
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (dupIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            VfxElement copy = subs[dupIdx];
            subs.insert(subs.begin() + dupIdx + 1, std::move(copy));
            CommitChange(b, "エレメント複製");
        }
        if (removeIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            subs.erase(subs.begin() + removeIdx);
            CommitChange(b, "エレメント削除");
        }
    }

} // namespace YoRigine
#endif
