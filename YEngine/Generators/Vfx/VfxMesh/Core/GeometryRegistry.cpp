// ===========================================================
// GeometryRegistry.cpp
// ===========================================================
#include "GeometryRegistry.h"

namespace YoRigine {

// ===========================================================
// シングルトン
// ===========================================================

GeometryRegistry& GeometryRegistry::Instance()
{
    static GeometryRegistry inst;
    return inst;
}

// ===========================================================
// 登録
// ===========================================================

void GeometryRegistry::Register(VfxGeometryDesc desc)
{
    for (auto& d : descs_) {
        if (d.type == desc.type) { d = std::move(desc); return; }
    }
    descs_.push_back(std::move(desc));
}

// ===========================================================
// 生成
// ===========================================================

std::unique_ptr<VfxGeometry> GeometryRegistry::Create(
    VfxGeometryType type, const VfxGeometryParams& params) const
{
    const auto* d = FindDesc(type);
    if (!d || !d->create) return nullptr;
    return d->create(params);
}

// ===========================================================
// 検索・デフォルト
// ===========================================================

const VfxGeometryDesc* GeometryRegistry::FindDesc(VfxGeometryType type) const
{
    for (const auto& d : descs_) {
        if (d.type == type) return &d;
    }
    return nullptr;
}

VfxGeometryParams GeometryRegistry::MakeDefault(VfxGeometryType type) const
{
    const auto* d = FindDesc(type);
    return d ? d->defaultParams : VfxGeometryParams{};
}

// ===========================================================
// エディタ UI
// ===========================================================

#ifdef USE_IMGUI
bool GeometryRegistry::DrawUI(VfxGeometryType type, VfxGeometryParams& params) const
{
    const auto* d = FindDesc(type);
    return (d && d->drawUI) ? d->drawUI(params) : false;
}
#endif

} // namespace YoRigine
