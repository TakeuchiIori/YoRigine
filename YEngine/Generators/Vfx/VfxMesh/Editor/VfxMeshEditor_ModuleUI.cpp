#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <imgui.h>
#include "Core/Editor/Widgets/YEditorWidget.h"

namespace YoRigine {

    namespace {
        void AddWaypointBeamModule(std::vector<VfxModule>& modules)
        {
            VfxModule beamPulse;
            beamPulse.type = VfxModuleType::BeamPulse;
            beamPulse.target = VfxModuleTarget::LightVolume;
            beamPulse.amplitude = 0.18f;
            beamPulse.frequency = 1.4f;
            modules.push_back(beamPulse);

            VfxModule flicker;
            flicker.type = VfxModuleType::Flicker;
            flicker.target = VfxModuleTarget::LightVolume;
            flicker.amplitude = 0.12f;
            flicker.frequency = 9.0f;
            modules.push_back(flicker);
        }
    }

    // モジュールリスト編集（エフェクト全体用とエレメント個別用で共用）
    void VfxMeshEditor::DrawModuleListUI(std::vector<VfxModule>& modules,
                                         bool showTarget, const char* commitLabel)
    {
        auto* sel = Selected();
        if (!sel) return;

        // コンボ用文字列配列（SameLine が絡むため明示幅を維持）
        static const char* kTypeNames[] = {
            "Lifetime (エフェクト寿命)", "Move (等速移動)", "Rise (上昇)", "ScalePulse (スケール脈動)",
            "ScaleOverLife (スケール変化)", "ColorOverLife (色変化)", "FadeInOut (フェード)",
            "Accelerate (加速/重力)", "Orbit (周回)", "Shake (揺れ)",
            "Visibility (表示期間)", "Flicker (明滅)", "BeamPulse (ビーム脈動)"
        };
        static const char* kEaseNames[] = {
            "Linear", "EaseIn (ゆっくり開始)", "EaseOut (ゆっくり終了)", "EaseInOut (両端)",
            "EaseOutCubic (強めブレーキ)", "EaseOutExpo (爆発向き)", "EaseOutBack (ポップ)"
        };
        static const char* kTargetNames[] = {
            "All (全効果)", "NoiseVolume", "LightningBolt", "ShockwaveRing", "LightVolume"
        };

        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(modules.size()); ++i) {
            ImGui::PushID(i);
            auto& m = modules[i];

            VfxEffectAsset b = sel->asset;
            bool c = false;

            int typeIdx = static_cast<int>(m.type);
            ImGui::SetNextItemWidth(220);
            if (ImGui::Combo("種類##mt", &typeIdx, kTypeNames, IM_ARRAYSIZE(kTypeNames))) {
                m.type = static_cast<VfxModuleType>(typeIdx);
                c = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("削除##m")) removeIdx = i;

            if (showTarget && m.type != VfxModuleType::Lifetime) {
                int tgtIdx = static_cast<int>(m.target);
                ImGui::SetNextItemWidth(220);
                if (ImGui::Combo("対象##mtg", &tgtIdx, kTargetNames, IM_ARRAYSIZE(kTargetNames))) {
                    m.target = static_cast<VfxModuleTarget>(tgtIdx);
                    c = true;
                }
            }

            if (m.type != VfxModuleType::Lifetime) {
                ImGui::SetNextItemWidth(90);
                c |= ImGui::DragFloat("開始遅延(秒)##mst", &m.startTime, 0.02f, 0.0f, 30.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90);
                c |= ImGui::DragFloat("持続時間(秒)##mwin", &m.window, 0.02f, 0.0f, 30.0f, "%.2f");
                YEditorWidget::ItemTooltip(
                    "このモジュールが効く長さ。\n"
                    "0 = 無限（Scale/Color はエフェクト寿命いっぱいで 0→1）\n"
                    ">0 = この秒数で完了（移動系はここで停止）");
            }

            if (m.type == VfxModuleType::ScaleOverLife || m.type == VfxModuleType::ColorOverLife) {
                int easeIdx = static_cast<int>(m.ease);
                ImGui::SetNextItemWidth(220);
                if (ImGui::Combo("カーブ##mease", &easeIdx, kEaseNames, IM_ARRAYSIZE(kEaseNames))) {
                    m.ease = static_cast<VfxEase>(easeIdx);
                    c = true;
                }
            }

            switch (m.type) {
            case VfxModuleType::Lifetime:
                c |= YEditorWidget::DragFloat("エフェクト寿命(秒)##md", m.duration, 0.02f, 0.05f, 10.0f, "%.2f");
                ImGui::TextDisabled("  ※エフェクト全体のワンショット寿命を決めるだけのモジュール（動きは無し）。");
                ImGui::TextDisabled("  ※各モジュールの「持続時間=0」はこの寿命いっぱいで 0→1 に進む。");
                break;
            case VfxModuleType::Move:
                c |= YEditorWidget::DragVec3("速度(/秒)##mv", m.velocity, 0.02f, -20.0f, 20.0f, "%.2f");
                break;
            case VfxModuleType::Rise:
                c |= YEditorWidget::DragFloat("上昇速度##mr", m.velocity.y, 0.02f, -20.0f, 20.0f, "%.2f");
                c |= YEditorWidget::DragFloat("係数##ma",     m.amplitude, 0.02f, 0.0f, 10.0f, "%.2f");
                break;
            case VfxModuleType::ScalePulse:
                c |= YEditorWidget::DragFloat("振幅##mp",      m.amplitude, 0.01f, 0.0f, 2.0f, "%.2f");
                c |= YEditorWidget::DragFloat("周波数(Hz)##mf", m.frequency, 0.05f, 0.0f, 20.0f, "%.2f");
                ImGui::TextDisabled("  ※スケールを sin で脈動させる（鼓動・ふくらみ）");
                break;
            case VfxModuleType::BeamPulse:
                c |= YEditorWidget::DragFloat("ビーム振幅##mbpa",   m.amplitude, 0.01f, 0.0f, 2.0f, "%.2f");
                c |= YEditorWidget::DragFloat("脈動回数(/秒)##mbpf", m.frequency, 0.05f, 0.0f, 20.0f, "%.2f");
                ImGui::TextDisabled("  ※LightVolume の beamRadius / beamGlow を揺らします");
                break;
            case VfxModuleType::ScaleOverLife:
                c |= YEditorWidget::DragFloat("開始スケール##mss", m.scaleStart, 0.01f, 0.0f, 20.0f, "%.2f");
                c |= YEditorWidget::DragFloat("終了スケール##mse", m.scaleEnd,   0.01f, 0.0f, 20.0f, "%.2f");
                ImGui::TextDisabled("  ※元サイズへの倍率。0→1.5 で「破裂して広がる」");
                break;
            case VfxModuleType::ColorOverLife:
                c |= YEditorWidget::ColorHDR("開始色(乗算)##mcs", m.colorStart);
                c |= YEditorWidget::ColorHDR("終了色(乗算)##mce", m.colorEnd);
                ImGui::TextDisabled("  ※元の色に掛ける倍率。白(1,1,1,1)=変化なし / >1 で Bloom 強化");
                break;
            case VfxModuleType::FadeInOut:
                c |= YEditorWidget::DragFloat("フェードイン(秒)##mfi",  m.fadeIn,  0.01f, 0.0f, 10.0f, "%.2f");
                c |= YEditorWidget::DragFloat("フェードアウト(秒)##mfo", m.fadeOut, 0.01f, 0.0f, 10.0f, "%.2f");
                ImGui::TextDisabled("  ※アウトは効果時間（0ならワンショット寿命）の終端から逆算");
                break;
            case VfxModuleType::Accelerate:
                c |= YEditorWidget::DragVec3("初速(/秒)##mav",   m.velocity,     0.02f, -50.0f, 50.0f, "%.2f");
                c |= YEditorWidget::DragVec3("加速度(/秒²)##maa", m.acceleration, 0.02f, -50.0f, 50.0f, "%.2f");
                ImGui::TextDisabled("  ※重力なら加速度 Y=-9.8。打ち上げは初速+Y & 加速度-Y");
                break;
            case VfxModuleType::Orbit:
                c |= YEditorWidget::DragVec3("回転軸##mox", m.velocity, 0.02f, -1.0f, 1.0f, "%.2f");
                c |= YEditorWidget::DragFloat("半径##mor",        m.amplitude, 0.02f, 0.0f, 20.0f, "%.2f");
                c |= YEditorWidget::DragFloat("回転数(/秒)##mof", m.frequency, 0.05f, -10.0f, 10.0f, "%.2f");
                break;
            case VfxModuleType::Shake:
                c |= YEditorWidget::DragFloat("振れ幅##msa", m.amplitude, 0.01f, 0.0f, 5.0f, "%.2f");
                c |= YEditorWidget::DragFloat("速さ(Hz)##msf", m.frequency, 0.1f,  0.0f, 60.0f, "%.1f");
                break;
            case VfxModuleType::Visibility:
                ImGui::TextDisabled("  ※開始遅延〜開始遅延+効果時間 の間だけ表示（効果時間0=以降ずっと表示）");
                break;
            case VfxModuleType::Flicker:
                c |= YEditorWidget::DragFloat("明滅の深さ##mfa",   m.amplitude, 0.01f, 0.0f, 1.0f, "%.2f");
                c |= YEditorWidget::DragFloat("明滅回数(/秒)##mff", m.frequency, 0.5f,  0.0f, 120.0f, "%.1f");
                break;
            }

            if (c) CommitChange(b, commitLabel);
            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeIdx >= 0) {
            VfxEffectAsset b = sel->asset;
            modules.erase(modules.begin() + removeIdx);
            CommitChange(b, "モジュール削除");
        }

        if (ImGui::Button("＋ モジュール追加")) {
            VfxEffectAsset b = sel->asset;
            modules.push_back(VfxModule{});
            CommitChange(b, "モジュール追加");
        }
        if (modules.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(動きなし = 従来挙動)");
        }
    }

    void VfxMeshEditor::DrawModuleSection()
    {
        auto* sel = Selected();
        if (!sel) return;

        ImGui::TextDisabled("エフェクト全体の動き（寿命・移動・脈動）をデータで定義。ゲームでも同じ動きで再生されます。");
        ImGui::TextDisabled("エレメント1個だけを動かしたい場合は、エレメントタブの各インスタンス内のモジュールを使ってください。");
        ImGui::Spacing();

        if (ImGui::Button("Waypoint Beam Module 追加")) {
            VfxEffectAsset b = sel->asset;
            AddWaypointBeamModule(sel->asset.modules);
            CommitChange(b, "Waypoint Beam Module 追加");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("LightVolume に BeamPulse/Flicker を追加");
        ImGui::Separator();

        DrawModuleListUI(sel->asset.modules, true, "モジュール編集");
    }

    void VfxMeshEditor::ApplyDefaultElementModules(VfxElement& sub)
    {
        switch (sub.type) {
        case VfxElementType::LightVolume:
            sub.lightVolume.beamStrength = 0.85f;
            sub.lightVolume.beamRadius = 0.32f;
            sub.lightVolume.beamPower = 2.5f;
            sub.lightVolume.beamGlow = 3.0f;
            sub.lightVolume.edgeFade = 0.08f;
            sub.lightVolume.depthFade = 50.0f;
            AddWaypointBeamModule(sub.modules);
            break;

        case VfxElementType::NoiseVolume: {
            sub.smoke.builtInBurstMotion = false;
            sub.smoke.riseSpeed = 0.f;
            sub.smoke.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            VfxModule grow;
            grow.type = VfxModuleType::ScaleOverLife;
            grow.ease = VfxEase::EaseOutExpo;
            grow.window = 2.0f;
            grow.scaleStart = 0.3f;
            grow.scaleEnd = 1.6f;
            sub.modules.push_back(grow);

            VfxModule rise;
            rise.type = VfxModuleType::Rise;
            rise.startTime = 0.2f;
            rise.window = 1.8f;
            rise.velocity = { 0.f, 1.0f, 0.f };
            rise.amplitude = 1.0f;
            sub.modules.push_back(rise);

            VfxModule fade;
            fade.type = VfxModuleType::FadeInOut;
            fade.window = 2.0f;
            fade.fadeIn = 0.05f;
            fade.fadeOut = 0.7f;
            sub.modules.push_back(fade);

            VfxModule color;
            color.type = VfxModuleType::ColorOverLife;
            color.ease = VfxEase::EaseOutCubic;
            color.window = 0.7f;
            color.colorStart = { 1.2f, 0.6f, 0.3f, 1.0f };
            color.colorEnd = { 0.22f, 0.22f, 0.24f, 1.0f };
            sub.modules.push_back(color);

            VfxModule pulse;
            pulse.type = VfxModuleType::ScalePulse;
            pulse.amplitude = 0.08f;
            pulse.frequency = 1.2f;
            sub.modules.push_back(pulse);

            VfxModule shake;
            shake.type = VfxModuleType::Shake;
            shake.amplitude = 0.06f;
            shake.frequency = 1.5f;
            sub.modules.push_back(shake);
            break;
        }

        default:
            break;
        }
    }

} // namespace YoRigine
#endif
