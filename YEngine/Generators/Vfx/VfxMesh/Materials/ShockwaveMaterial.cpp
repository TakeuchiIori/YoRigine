// ===========================================================
// ShockwaveMaterial.cpp
// ===========================================================
#include "ShockwaveMaterial.h"
#include <Vfx/VfxMesh/Effects/ShockwaveMesh.h> // ShockwaveParamsCB
#ifdef USE_IMGUI
#include "Core/Editor/Widgets/YEditorWidget.h"
#endif

namespace YoRigine {

static constexpr size_t kCBAlign = 256;
template<typename T>
static constexpr size_t CBSize256() { return (sizeof(T) + kCBAlign - 1) & ~(kCBAlign - 1); }

ShockwaveMaterial::ShockwaveMaterial(const ShockwaveMatParams& param) : param_(param) {}

size_t ShockwaveMaterial::GetCBByteSize() const { return CBSize256<ShockwaveParamsCB>(); }

// ===========================================================
// 定数バッファ書き込み
// ===========================================================

void ShockwaveMaterial::FillCB(void* mapped, const VfxMatFillArgs& args) const
{
    auto& cb = *static_cast<ShockwaveParamsCB*>(mapped);

    // tint（モジュール評価結果）を元の色に乗算。a に不透明度が乗るのでフェードも兼ねる。
    const auto& t = args.tint;
    cb.color = {
        param_.color.x * t.x,
        param_.color.y * t.y,
        param_.color.z * t.z,
        param_.color.w * t.w
    };
    cb.thickness  = param_.thickness;
    cb.ringRadius = param_.ringRadius;
    cb._pad0 = cb._pad1 = 0.f;
    // 膨張は ScaleOverLife モジュールがジオメトリを拡大して表現する（シェーダーは固定半径）
}

// ===========================================================
// MaterialRegistry 登録情報
// ===========================================================

VfxMaterialDesc ShockwaveMaterial::Describe()
{
    VfxMaterialDesc d;
    d.type          = VfxMaterialType::Shockwave;
    d.displayName   = "Shockwave";
    d.defaultParams = ShockwaveMatParams{};

    d.create = [](const VfxMaterialParams& p) -> std::unique_ptr<VfxMaterial> {
        return std::make_unique<ShockwaveMaterial>(std::get<ShockwaveMatParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxMaterialParams& params) -> bool {
        auto& p = std::get<ShockwaveMatParams>(params);
        bool c = false;
        c |= YEditorWidget::ColorHDR("色(HDR暖色)##shock", p.color);
        c |= YEditorWidget::SliderFloat("リングの太さ##shock", p.thickness, 0.01f, 1.f, "%.2f");
        c |= YEditorWidget::SliderFloat("リング位置##shock", p.ringRadius, 0.1f, 1.f, "%.2f");
        ImGui::TextDisabled("  ※膨張は ScaleOverLife モジュールで作ります");
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine
