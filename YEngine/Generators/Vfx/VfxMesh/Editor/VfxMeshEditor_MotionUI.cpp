#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <imgui.h>

namespace YoRigine {

    namespace {
        void AddWaypointBeamMotion(std::vector<VfxMotion>& motions)
        {
            VfxMotion pulse;
            pulse.type = VfxMotionType::Pulse;
            pulse.target = VfxMotionTarget::LightVolume;
            pulse.amplitude = 0.04f;
            pulse.frequency = 1.6f;
            motions.push_back(pulse);

            VfxMotion flicker;
            flicker.type = VfxMotionType::Flicker;
            flicker.target = VfxMotionTarget::LightVolume;
            flicker.amplitude = 0.12f;
            flicker.frequency = 9.0f;
            motions.push_back(flicker);
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

            if (m.type == VfxMotionType::ScaleOverLife || m.type == VfxMotionType::ColorOverLife) {
                int easeIdx = static_cast<int>(m.ease);
                ImGui::SetNextItemWidth(220);
                if (ImGui::Combo("カーブ##mease", &easeIdx, easeNames, IM_ARRAYSIZE(easeNames))) {
                    m.ease = static_cast<VfxEase>(easeIdx);
                    c = true;
                }
            }

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
                ImGui::TextDisabled("  ※元の色に掛ける倍率。白(1,1,1,1)=変化なし / >1 で Bloom 強化");
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

        if (ImGui::Button("Waypoint Beam Motion 追加")) {
            VfxEffectAsset b = sel->asset;
            AddWaypointBeamMotion(sel->asset.motions);
            CommitChange(b, "Waypoint Beam Motion 追加");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("LightVolume に Pulse/Flicker を追加");
        ImGui::Separator();

        DrawMotionListUI(sel->asset.motions, true, "モーション編集");
    }

    void VfxMeshEditor::ApplyDefaultSubEffectMotions(VfxSubEffect& sub)
    {
        switch (sub.type) {
        case VfxSubEffectType::LightVolume:
            sub.lightVolume.beamStrength = 0.85f;
            sub.lightVolume.beamRadius = 0.32f;
            sub.lightVolume.beamPower = 2.5f;
            sub.lightVolume.beamGlow = 3.0f;
            sub.lightVolume.edgeFade = 0.08f;
            sub.lightVolume.depthFade = 50.0f;
            AddWaypointBeamMotion(sub.motions);
            break;

        case VfxSubEffectType::Smoke: {
            sub.smoke.builtInBurstMotion = false;
            sub.smoke.riseSpeed = 0.f;
            sub.smoke.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            VfxMotion grow;
            grow.type = VfxMotionType::ScaleOverLife;
            grow.ease = VfxEase::EaseOutExpo;
            grow.window = 2.0f;
            grow.scaleStart = 0.3f;
            grow.scaleEnd = 1.6f;
            sub.motions.push_back(grow);

            VfxMotion rise;
            rise.type = VfxMotionType::Rise;
            rise.startTime = 0.2f;
            rise.window = 1.8f;
            rise.velocity = { 0.f, 1.0f, 0.f };
            rise.amplitude = 1.0f;
            sub.motions.push_back(rise);

            VfxMotion fade;
            fade.type = VfxMotionType::FadeInOut;
            fade.window = 2.0f;
            fade.fadeIn = 0.05f;
            fade.fadeOut = 0.7f;
            sub.motions.push_back(fade);

            VfxMotion color;
            color.type = VfxMotionType::ColorOverLife;
            color.ease = VfxEase::EaseOutCubic;
            color.window = 0.7f;
            color.colorStart = { 1.2f, 0.6f, 0.3f, 1.0f };
            color.colorEnd = { 0.22f, 0.22f, 0.24f, 1.0f };
            sub.motions.push_back(color);

            VfxMotion pulse;
            pulse.type = VfxMotionType::Pulse;
            pulse.amplitude = 0.08f;
            pulse.frequency = 1.2f;
            sub.motions.push_back(pulse);

            VfxMotion shake;
            shake.type = VfxMotionType::Shake;
            shake.amplitude = 0.06f;
            shake.frequency = 1.5f;
            sub.motions.push_back(shake);
            break;
        }

        default:
            break;
        }
    }

} // namespace YoRigine
#endif
