// ===========================================================
// MaterialRegistry.cpp
// ===========================================================
#include "MaterialRegistry.h"

namespace YoRigine {

// ===========================================================
// シングルトン
// ===========================================================

MaterialRegistry& MaterialRegistry::Instance()
{
    static MaterialRegistry inst;
    return inst;
}

// ===========================================================
// 登録
// ===========================================================

void MaterialRegistry::Register(VfxMaterialDesc desc)
{
    for (auto& d : descs_) {
        if (d.type == desc.type) { d = std::move(desc); return; }
    }
    descs_.push_back(std::move(desc));
}

// ===========================================================
// 生成
// ===========================================================

std::unique_ptr<VfxMaterial> MaterialRegistry::Create(
    VfxMaterialType type, const VfxMaterialParams& params) const
{
    const auto* d = FindDesc(type);
    if (!d || !d->create) return nullptr;
    return d->create(params);
}

// ===========================================================
// 検索・デフォルト
// ===========================================================

const VfxMaterialDesc* MaterialRegistry::FindDesc(VfxMaterialType type) const
{
    for (const auto& d : descs_) {
        if (d.type == type) return &d;
    }
    return nullptr;
}

VfxMaterialParams MaterialRegistry::MakeDefault(VfxMaterialType type) const
{
    const auto* d = FindDesc(type);
    return d ? d->defaultParams : VfxMaterialParams{};
}

// ===========================================================
// エディタ UI
// ===========================================================

#ifdef USE_IMGUI
bool MaterialRegistry::DrawUI(VfxMaterialType type, VfxMaterialParams& params) const
{
    const auto* d = FindDesc(type);
    return (d && d->drawUI) ? d->drawUI(params) : false;
}
#endif

} // namespace YoRigine
