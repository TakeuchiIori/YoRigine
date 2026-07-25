#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include "Core/Editor/Widgets/YEditorWidget.h"

namespace YoRigine {

    void VfxMeshEditor::DrawTrailSection()
    {
        auto* sel = Selected();
        if (!sel) return;
        auto& t = sel->asset.trail;

        YEditorWidget::SectionHeader("形状設定");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;

            static const char* kShapeNames[] = {
                "Flat (平板)", "Arc (円弧)", "Fan (扇形)",
                "Custom (2D ポリゴン押し出し)", "Primitive (3D メッシュ)"
            };
            int shapeIdx = static_cast<int>(t.shapeType);
            if (YEditorWidget::Combo("##shape", shapeIdx, kShapeNames, IM_ARRAYSIZE(kShapeNames))) {
                t.shapeType = static_cast<TrailShapeType>(shapeIdx);
                c = true;
            }
            ImGui::SameLine(0, 4);
            ImGui::TextDisabled("断面形状");

            if (t.shapeType == TrailShapeType::Arc || t.shapeType == TrailShapeType::Fan) {
                c |= YEditorWidget::SliderInt("幅の分割数##wseg", t.widthSegments, 1, 16);
                c |= YEditorWidget::SliderFloat("円弧の角度(度)##arcang", t.arcAngleDeg, 10.f, 360.f, "%.1f");
            }
            c |= YEditorWidget::DragInt("滑らかさ(補間分割数)##spline", t.splineSubdivisions, 1.f, 1, 512);

            if (c) CommitChange(b, "Trail 形状設定");
        }

        if (t.shapeType == TrailShapeType::Primitive) {
            YEditorWidget::SectionHeader("3D プリミティブ");
            VfxEffectAsset b = sel->asset;
            bool c = false;
            auto& sp = t.primitive;

            static const char* kTypeNames[] = { "Box", "Sphere", "Capsule", "Cone", "Cylinder", "Torus" };
            int typeIdx = static_cast<int>(sp.type);
            ImGui::SetNextItemWidth(180);
            if (ImGui::Combo("Type##prim", &typeIdx, kTypeNames, IM_ARRAYSIZE(kTypeNames))) {
                sp.type = static_cast<PrimitiveType>(typeIdx);
                c = true;
            }

            static const char* kPlaceNames[] = { "Static (1個固定)", "BeadAlongTrail (軌跡に連続配置)" };
            int placeIdx = static_cast<int>(sp.placement);
            ImGui::SetNextItemWidth(260);
            if (ImGui::Combo("配置##prim", &placeIdx, kPlaceNames, IM_ARRAYSIZE(kPlaceNames))) {
                sp.placement = static_cast<PrimitivePlacement>(placeIdx);
                c = true;
            }

            c |= YEditorWidget::DragFloat("スタンプスケール##prim", sp.stampScale, 0.01f, 0.001f, 100.f, "%.3f");

            ImGui::Spacing();
            switch (sp.type) {
            case PrimitiveType::Box:
                c |= YEditorWidget::DragVec3("半辺長##bx", sp.halfExtents, 0.01f, 0.001f, 10.f, "%.3f");
                break;
            case PrimitiveType::Sphere:
                c |= YEditorWidget::DragFloat("半径##sp", sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= YEditorWidget::SliderInt("緯度分割##sp", sp.latSegments, 2, 64);
                c |= YEditorWidget::SliderInt("経度分割##sp", sp.lonSegments, 3, 64);
                break;
            case PrimitiveType::Capsule:
                c |= YEditorWidget::DragFloat("半径##cap", sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= YEditorWidget::DragFloat("全高##cap", sp.height, 0.01f, 0.001f, 20.f, "%.3f");
                c |= YEditorWidget::SliderInt("緯度分割##cap", sp.latSegments, 2, 32);
                c |= YEditorWidget::SliderInt("経度分割##cap", sp.lonSegments, 3, 32);
                break;
            case PrimitiveType::Cone:
                c |= YEditorWidget::DragFloat("底面半径##cn", sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= YEditorWidget::DragFloat("高さ##cn", sp.height, 0.01f, 0.001f, 20.f, "%.3f");
                c |= YEditorWidget::SliderInt("分割##cn", sp.lonSegments, 3, 64);
                break;
            case PrimitiveType::Cylinder:
                c |= YEditorWidget::DragFloat("半径##cy", sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= YEditorWidget::DragFloat("高さ##cy", sp.height, 0.01f, 0.001f, 20.f, "%.3f");
                c |= YEditorWidget::SliderInt("分割##cy", sp.lonSegments, 3, 64);
                break;
            case PrimitiveType::Torus:
                c |= YEditorWidget::DragFloat("主半径 R##to", sp.radius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= YEditorWidget::DragFloat("管半径 r##to", sp.tubeRadius, 0.01f, 0.001f, 10.f, "%.3f");
                c |= YEditorWidget::SliderInt("主分割##to", sp.lonSegments, 3, 64);
                c |= YEditorWidget::SliderInt("管分割##to", sp.ringSegments, 3, 32);
                break;
            }

            if (sp.placement == PrimitivePlacement::BeadAlongTrail) {
                ImGui::Spacing();
                ImGui::TextDisabled("Bead モード設定");
                c |= YEditorWidget::DragFloat("間引き間隔##bead", sp.stampSpacing, 0.01f, 0.0f, 10.0f, "%.3f");
                YEditorWidget::ItemTooltip("0 = 平滑化後の全点に配置 / >0 = 直線距離で間引き");
                c |= ImGui::Checkbox("寿命でスケール縮小##bead", &sp.scaleByAge);
            }

            if (c) CommitChange(b, "Primitive 設定");
        }

        YEditorWidget::SectionHeader("幅");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= YEditorWidget::SliderFloat("根元幅##ws", t.widthStart, 0.f, 5.f, "%.3f");
            c |= YEditorWidget::SliderFloat("先端幅##we", t.widthEnd, 0.f, 5.f, "%.3f");
            if (c) CommitChange(b, "Trail 幅");
        }

        YEditorWidget::SectionHeader("寿命 / ポイント数");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= YEditorWidget::SliderFloat("寿命 (秒)##lt", t.lifetime, 0.05f, 5.f, "%.2f");
            c |= YEditorWidget::SliderInt("最大ポイント##mp", t.maxPoints, 4, 512);
            if (c) CommitChange(b, "Trail 寿命");
        }

        YEditorWidget::SectionHeader("カラーグラデーション");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= YEditorWidget::ColorHDR("根元カラー##cs", t.colorStart);
            c |= YEditorWidget::ColorHDR("先端カラー##ce", t.colorEnd);
            if (c) CommitChange(b, "Trail カラー");
            ImGui::TextDisabled("  ※値を >1 にすると Bloom で強く発光する");
        }

        YEditorWidget::SectionHeader("発光");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= YEditorWidget::SliderFloat("発光強度 (0で消灯)##emi", t.emissiveIntensity, 0.f, 5.f, "%.2f");
            c |= YEditorWidget::SliderFloat("中心グロー##glow", t.glowPower, 0.f, 8.f, "%.2f");
            c |= YEditorWidget::SliderFloat("エッジソフト##sof", t.softness, 0.f, 1.f, "%.2f");
            if (c) CommitChange(b, "Trail 発光");
            ImGui::TextDisabled("  ※発光強度=0 で完全に消灯（形だけ確認したい時に）");
        }

        YEditorWidget::SectionHeader("アウトライン強調 (縁の発光)");
        {
            VfxEffectAsset b = sel->asset;
            bool c = false;
            c |= YEditorWidget::ColorHDR("縁の色##rim", t.rimColor);
            c |= YEditorWidget::SliderFloat("縁の強さ##rim", t.fresnelStrength, 0.f, 5.f, "%.2f");
            c |= YEditorWidget::SliderFloat("縁の細さ##rim", t.trailSharpness, 0.2f, 8.f, "%.2f");
            if (c) CommitChange(b, "Trail アウトライン");
        }

        YEditorWidget::SectionHeader("ブレンドモード");
        {
            VfxEffectAsset b = sel->asset;
            static const char* kBlendNames[] = { "通常", "加算", "減算", "乗算" };
            int idx = static_cast<int>(t.blendMode);
            if (YEditorWidget::Combo("##blend", idx, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
                t.blendMode = static_cast<BlendMode>(idx);
                CommitChange(b, "Trail ブレンド");
            }
        }

        YEditorWidget::SectionHeader("シェーダー");
        {
            VfxEffectAsset b = sel->asset;
            if (YEditorWidget::SliderFloat("UV スクロール速度##uvs", t.uvScrollSpeed, -5.f, 5.f, "%.2f"))
                CommitChange(b, "Trail UV スクロール");
        }

        YEditorWidget::SectionHeader("テクスチャ");
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
        if (ImGui::BeginPopupModal("##RampTexSelect", &showRampPopup_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
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
        if (ImGui::BeginPopupModal("##NoiseTexSelect", &showNoisePopup_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
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

} // namespace YoRigine
#endif
