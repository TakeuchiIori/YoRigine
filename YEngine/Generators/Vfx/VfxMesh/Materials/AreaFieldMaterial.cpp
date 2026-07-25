// ===========================================================
// AreaFieldMaterial.cpp
// ===========================================================
#include "AreaFieldMaterial.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <IconsFontAwesome5.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr size_t kCBAlign = 256;
template<typename T>
static constexpr size_t CBSize256() { return (sizeof(T) + kCBAlign - 1) & ~(kCBAlign - 1); }

AreaFieldMaterial::AreaFieldMaterial(const AreaFieldMatParams& param) : param_(param) {}

size_t AreaFieldMaterial::GetCBByteSize() const { return CBSize256<AreaFieldParamsCB>(); }

// ===========================================================
// 定数バッファ書き込み
// ===========================================================

void AreaFieldMaterial::FillCB(void* mapped, const VfxMatFillArgs& args) const
{
    auto& cb = *static_cast<AreaFieldParamsCB*>(mapped);

    // tint（モジュール評価＋インスタンス色）を元の色に乗算。a に不透明度が乗る。
    const auto& t = args.tint;
    cb.color = {
        param_.color.x * t.x,
        param_.color.y * t.y,
        param_.color.z * t.z,
        param_.color.w * t.w
    };
    // 縁の色相はインスタンス色に追従させる（明度はリム側の値を維持）。a は塗りと共通の不透明度。
    cb.edgeColor = {
        param_.edgeColor.x * t.x,
        param_.edgeColor.y * t.y,
        param_.edgeColor.z * t.z,
        param_.color.w * t.w
    };
    cb.fillOpacity = param_.fillOpacity;
    cb.edgeWidth   = param_.edgeWidth;
    cb.ringSpeed   = param_.ringSpeed;
    cb.noiseScale  = param_.noiseScale;
    cb.scanSpeed   = param_.scanSpeed;
    cb.runeCount   = param_.runeCount;
    cb.styleRune   = param_.styleRune;
    cb.styleEnergy = param_.styleEnergy;
    cb.styleScan   = param_.styleScan;
    cb.time        = args.age;   // アニメ時間（回転・スキャン・ノイズ流れ）
    cb.style       = static_cast<float>(param_.style);
    cb.fillStyle   = static_cast<float>(param_.fillStyle);
}

#ifdef USE_IMGUI
// スタイルごとの推奨プリセット（対称数・回転・ブレンド・色）。ボタン1つで流し込む。
// style は据え置き、それ以外を上書きする。色は量産時 SetColor で上書きされるが
// プレビュー用に見栄えする既定を入れておく。
static void ApplyAreaFieldStylePreset(AreaFieldMatParams& p)
{
    switch (p.style) {
    default:
    case 0: // 古典魔法陣
        p.runeCount = 6.f; p.ringSpeed = 0.50f; p.styleRune = 1.f; p.styleEnergy = 0.50f; p.styleScan = 0.30f;
        p.color = { 2.0f, 1.4f, 0.5f, 1.f }; p.edgeColor = { 2.6f, 1.9f, 0.8f, 1.f }; break;
    case 1: // 同心円
        p.runeCount = 8.f; p.ringSpeed = 0.40f; p.styleRune = 1.f; p.styleEnergy = 0.20f; p.styleScan = 0.60f;
        p.color = { 0.40f, 1.6f, 2.2f, 1.f }; p.edgeColor = { 0.8f, 2.2f, 2.8f, 1.f }; break;
    case 2: // ルーン時計
        p.runeCount = 12.f; p.ringSpeed = 0.30f; p.styleRune = 1.f; p.styleEnergy = 0.15f; p.styleScan = 0.20f;
        p.color = { 2.2f, 1.2f, 0.3f, 1.f }; p.edgeColor = { 2.8f, 1.7f, 0.6f, 1.f }; break;
    case 3: // 六芒星
        p.runeCount = 6.f; p.ringSpeed = 0.20f; p.styleRune = 1.f; p.styleEnergy = 0.25f; p.styleScan = 0.20f;
        p.color = { 1.4f, 0.6f, 2.2f, 1.f }; p.edgeColor = { 2.0f, 1.1f, 2.8f, 1.f }; break;
    case 4: // 五角星印
        p.runeCount = 5.f; p.ringSpeed = 0.25f; p.styleRune = 1.f; p.styleEnergy = 0.20f; p.styleScan = 0.20f;
        p.color = { 2.2f, 0.5f, 0.4f, 1.f }; p.edgeColor = { 2.8f, 0.9f, 0.7f, 1.f }; break;
    case 5: // 花弁
        p.runeCount = 8.f; p.ringSpeed = 0.50f; p.styleRune = 1.f; p.styleEnergy = 0.40f; p.styleScan = 0.30f;
        p.color = { 2.2f, 0.8f, 1.4f, 1.f }; p.edgeColor = { 2.8f, 1.3f, 1.9f, 1.f }; break;
    case 6: // レーダー
        p.runeCount = 8.f; p.ringSpeed = 0.80f; p.styleRune = 1.f; p.styleEnergy = 0.10f; p.styleScan = 0.50f;
        p.color = { 0.40f, 2.0f, 0.8f, 1.f }; p.edgeColor = { 0.9f, 2.6f, 1.3f, 1.f }; break;
    case 7: // 秘術
        p.runeCount = 6.f; p.ringSpeed = 0.30f; p.styleRune = 1.f; p.styleEnergy = 0.60f; p.styleScan = 0.40f;
        p.noiseScale = 4.f;
        p.color = { 0.8f, 1.2f, 2.2f, 1.f }; p.edgeColor = { 1.3f, 1.8f, 2.8f, 1.f }; break;
    }
}
#endif

// ===========================================================
// MaterialRegistry 登録情報
// ===========================================================

VfxMaterialDesc AreaFieldMaterial::Describe()
{
    VfxMaterialDesc d;
    d.type          = VfxMaterialType::AreaField;
    d.displayName   = "AreaField";
    d.defaultParams = AreaFieldMatParams{};

    d.create = [](const VfxMaterialParams& p) -> std::unique_ptr<VfxMaterial> {
        return std::make_unique<AreaFieldMaterial>(std::get<AreaFieldMatParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxMaterialParams& params) -> bool {
        auto& p = std::get<AreaFieldMatParams>(params);
        bool c = false;
        // 魔法陣デザイン選択（8種）＋推奨プリセット
        static const char* kStyleNames[] = {
            "0 古典魔法陣", "1 同心円", "2 ルーン時計", "3 六芒星",
            "4 五角星印", "5 花弁", "6 レーダー", "7 秘術"
        };
        int s = p.style;
        if (s < 0) s = 0; if (s > 7) s = 7;
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("魔法陣##areastyle", &s, kStyleNames, IM_ARRAYSIZE(kStyleNames))) {
            p.style = s; c = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("推奨プリセット##area")) { ApplyAreaFieldStylePreset(p); c = true; }

        // 内部の質感（魔法陣の線とは独立した軸）
        static const char* kFillNames[] = {
            "0 無地", "1 毒(どくどく)", "2 バフ(上昇)", "3 エネルギー", "4 波紋", "5 残り火"
        };
        int fs = p.fillStyle;
        if (fs < 0) fs = 0; if (fs > 5) fs = 5;
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("内部の質感##areafill", &fs, kFillNames, IM_ARRAYSIZE(kFillNames))) {
            p.fillStyle = fs; c = true;
        }
        ImGui::TextDisabled("  魔法陣(線)と内部(面)は独立。例: 同心円×毒 / 六芒星×バフ");
        ImGui::Separator();

        c |= YEditorWidget::ColorHDR("ベース色(HDR)##area", p.color);
        c |= YEditorWidget::ColorHDR("外周リム色(HDR)##area", p.edgeColor);
        c |= YEditorWidget::SliderFloat("内部の濃さ##area", p.fillOpacity, 0.f, 1.5f, "%.2f");
        c |= YEditorWidget::SliderFloat("外周リム幅##area", p.edgeWidth, 0.005f, 0.5f, "%.3f");
        ImGui::Separator();
        ImGui::TextDisabled("重ねがけ（魔法陣＋エネルギー＋スキャンをブレンド）");
        c |= YEditorWidget::SliderFloat(ICON_FA_CIRCLE " 魔法陣の強さ##area", p.styleRune,   0.f, 1.f, "%.2f");
        c |= YEditorWidget::SliderFloat(ICON_FA_CIRCLE " エネルギー場##area", p.styleEnergy, 0.f, 1.f, "%.2f");
        c |= YEditorWidget::SliderFloat(ICON_FA_CIRCLE " スキャン波##area",   p.styleScan,   0.f, 1.f, "%.2f");
        ImGui::Separator();
        c |= YEditorWidget::SliderFloat("回転速度##area",     p.ringSpeed, -3.f, 3.f, "%.2f");
        c |= YEditorWidget::SliderFloat("対称数/辺数##area",  p.runeCount, 1.f, 16.f, "%.0f");
        c |= YEditorWidget::SliderFloat("ノイズ密度##area",   p.noiseScale, 0.5f, 8.f, "%.2f");
        c |= YEditorWidget::SliderFloat("スキャン速度##area", p.scanSpeed, 0.f, 4.f, "%.2f");
        ImGui::TextDisabled("  ※膨張は element scale（Disc）が担当");
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine
