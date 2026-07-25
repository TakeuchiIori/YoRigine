// ===========================================================
// VfxMeshRegistry.cpp
// ===========================================================
#include "VfxMeshRegistry.h"

namespace YoRigine {

// ===========================================================
// シングルトン
// ===========================================================

VfxMeshRegistry& VfxMeshRegistry::Instance()
{
    static VfxMeshRegistry inst;
    return inst;
}

// ===========================================================
// 登録
// ===========================================================

void VfxMeshRegistry::Register(VfxElementDesc desc)
{
    // 同一 type が既に登録済みなら上書き（重複登録を許容）
    for (auto& d : descs_) {
        if (d.type == desc.type) { d = std::move(desc); return; }
    }
    descs_.push_back(std::move(desc));
}

// ===========================================================
// 生成
// ===========================================================

std::unique_ptr<ProceduralMeshBase> VfxMeshRegistry::Create(
    const VfxElement& def, const Vector3& basePos, float scale, Camera* camera) const
{
    const auto* d = FindDesc(def.type);
    if (!d || !d->create) return nullptr;
    return d->create(def, basePos, scale, camera);
}

// ===========================================================
// 検索
// ===========================================================

const VfxElementDesc* VfxMeshRegistry::FindDesc(VfxElementType type) const
{
    for (const auto& d : descs_) {
        if (d.type == type) return &d;
    }
    return nullptr;
}

// ===========================================================
// エディタ UI
// ===========================================================

#ifdef USE_IMGUI
void VfxMeshRegistry::DrawUI(VfxElementType type, VfxElement& sub,
                              const VfxEffectAsset& asset,
                              const VfxElementDesc::CommitFn& commit) const
{
    const auto* d = FindDesc(type);
    if (d && d->drawUI) d->drawUI(sub, asset, commit);
}
#endif

} // namespace YoRigine
