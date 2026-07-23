// ===========================================================
// NoiseMaterial.cpp
// ===========================================================
#include "NoiseMaterial.h"
#include <Vfx/VfxMesh/Effects/VolumeSmokeMesh.h> // SmokeParamsCB
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr size_t kCBAlign = 256;
template<typename T>
static constexpr size_t CBSize256() { return (sizeof(T) + kCBAlign - 1) & ~(kCBAlign - 1); }

NoiseMaterial::NoiseMaterial(const NoiseMatParams& param) : param_(param) {}

size_t NoiseMaterial::GetCBByteSize() const { return CBSize256<SmokeParamsCB>(); }

// ===========================================================
// 定数バッファ書き込み
// ===========================================================

void NoiseMaterial::FillCB(void* mapped, const VfxMatFillArgs& args) const
{
    auto& cb = *static_cast<SmokeParamsCB*>(mapped);

    // tint（モジュール評価結果）を元の色に乗算する
    const auto& t = args.tint;
    cb.color = {
        param_.color.x * t.x,
        param_.color.y * t.y,
        param_.color.z * t.z,
        param_.color.w * t.w
    };
    cb.smokeColor   = param_.smokeColor;
    cb.center       = args.position;                 // ジオメトリの現在中心
    cb.radius       = param_.radius * args.scale;    // ワールド半径
    cb.time         = args.age;
    cb.noiseScale   = param_.noiseScale;
    cb.noiseStrength = param_.noiseStrength;
    cb.scrollSpeed  = param_.scrollSpeed;
    cb.fresnelPower = param_.fresnelPower;
    cb.density      = param_.density;
    cb.noiseOctaves = param_.noiseOctaves;
    cb.rimIntensity = param_.rimIntensity;
    cb.burst        = args.burstProgress;
    cb._pad2[0] = cb._pad2[1] = cb._pad2[2] = 0.f;
}

// ===========================================================
// MaterialRegistry 登録情報
// ===========================================================

VfxMaterialDesc NoiseMaterial::Describe()
{
    VfxMaterialDesc d;
    d.type          = VfxMaterialType::Noise;
    d.displayName   = "Noise";
    d.defaultParams = NoiseMatParams{};

    d.create = [](const VfxMaterialParams& p) -> std::unique_ptr<VfxMaterial> {
        return std::make_unique<NoiseMaterial>(std::get<NoiseMatParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxMaterialParams& params) -> bool {
        auto& p = std::get<NoiseMatParams>(params);
        bool c = false;
        c |= YEditorWidget::ColorHDR("色(rgb>1でBloom)##noise", p.color);
        c |= YEditorWidget::ColorHDR("バースト後の煙色##noise", p.smokeColor);
        c |= YEditorWidget::DragFloat("フレネル基準半径##noise", p.radius, 0.05f, 0.05f, 50.f, "%.2f");
        c |= YEditorWidget::DragFloat("ノイズタイリング##noise", p.noiseScale, 0.05f, 0.1f, 20.f, "%.2f");
        c |= YEditorWidget::SliderFloat("ノイズ強度##noise", p.noiseStrength, 0.f, 1.f, "%.2f");
        c |= YEditorWidget::DragFloat("スクロール速度##noise", p.scrollSpeed, 0.01f, 0.f, 5.f, "%.2f");
        c |= YEditorWidget::DragFloat("フレネル鋭さ##noise", p.fresnelPower, 0.05f, 0.1f, 10.f, "%.2f");
        c |= YEditorWidget::SliderFloat("密度##noise", p.density, 0.f, 4.f, "%.2f");
        c |= YEditorWidget::SliderFloat("オクターブ##noise", p.noiseOctaves, 1.f, 4.f, "%.1f");
        c |= YEditorWidget::DragFloat("リム発光##noise", p.rimIntensity, 0.05f, 0.f, 8.f, "%.2f");
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine
