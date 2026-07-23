#pragma once
// ===========================================================
// PlaneGeometry.h
//
// カメラを向く正方形クワッド（板ポリ）を生成するジオメトリ。
// 地面への衝撃デカール・魔法陣・光輪などの平面エフェクト向け。
// カメラ方向が変わるため毎フレーム再構築する。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class PlaneGeometry : public VfxGeometry
    {
    public:
        explicit PlaneGeometry(const PlaneGeomParams& params);

        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        bool IsDirty(const VfxGeomState&, const VfxGeomState&) const override { return true; }

        void ApplyParams(const VfxGeometryParams& p) override {
            const auto& pl = std::get<PlaneGeomParams>(p);
            size_      = pl.size;
            billboard_ = pl.billboard;
        }

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float size_;
        bool  billboard_;
    };

} // namespace YoRigine
