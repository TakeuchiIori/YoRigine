#pragma once
// ===========================================================
// ShockwaveMaterial.h
//
// 衝撃波リング膨張シェーダ（"VfxMeshShockwave" PSO）を使うマテリアル。
// RingGeometry と組み合わせて爆発の衝撃波リングを表現する。
//
// パラメータは VfxMaterialTypes.h の ShockwaveMatParams を使う。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxMaterial.h>
#include <Vfx/VfxMesh/Core/VfxMaterialTypes.h>

namespace YoRigine {

    class ShockwaveMaterial : public VfxMaterial
    {
    public:
        explicit ShockwaveMaterial(const ShockwaveMatParams& param);

        const char* GetPSOName()    const override { return "VfxMeshShockwave"; }
        size_t      GetCBByteSize() const override;
        void        FillCB(void* mapped, const VfxMatFillArgs& args) const override;

        void ApplyParams(const VfxMaterialParams& p) override {
            param_ = std::get<ShockwaveMatParams>(p);
        }

        /// MaterialRegistry 用の記述子を返す
        static VfxMaterialDesc Describe();

    private:
        ShockwaveMatParams param_;
    };

} // namespace YoRigine
