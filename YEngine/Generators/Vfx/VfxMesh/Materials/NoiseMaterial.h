#pragma once
// ===========================================================
// NoiseMaterial.h
//
// FBM ノイズ渦巻きシェーダ（"VfxMeshSmoke" PSO）を使うマテリアル。
// スモーク・炎・オーラ・エネルギー球など幅広い表現に使える。
// SphereGeometry / ConeGeometry / PlaneGeometry など
// どのジオメトリとも組み合わせ可能。
//
// パラメータは VfxMaterialTypes.h の NoiseMatParams を使う。
// フレネルの中心・半径は FillCB 時にジオメトリの現在位置から算出する。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxMaterial.h>
#include <Vfx/VfxMesh/Core/VfxMaterialTypes.h>

namespace YoRigine {

    class NoiseMaterial : public VfxMaterial
    {
    public:
        explicit NoiseMaterial(const NoiseMatParams& param);

        const char* GetPSOName()    const override { return "VfxMeshSmoke"; }
        size_t      GetCBByteSize() const override;
        void        FillCB(void* mapped, const VfxMatFillArgs& args) const override;

        void ApplyParams(const VfxMaterialParams& p) override {
            param_ = std::get<NoiseMatParams>(p);
        }

        /// GeometryRegistry と対になる MaterialRegistry 用の記述子を返す
        static VfxMaterialDesc Describe();

    private:
        NoiseMatParams param_;
    };

} // namespace YoRigine
