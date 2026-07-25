#pragma once
// ===========================================================
// ConeGeometry.h
//
// 円錐メッシュを生成するジオメトリ。
// 炎・竜巻下部・噴射エフェクトなど向け。
// Y+ 方向が頂点（先端）になるよう生成する。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class ConeGeometry : public VfxGeometry
    {
    public:
        explicit ConeGeometry(const ConeGeomParams& params);

        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        /// 位置・スケール・回転のいずれかが変わった時だけ再構築する
        bool IsDirty(const VfxGeomState& prev,
                     const VfxGeomState& curr) const override;

        void ApplyParams(const VfxGeometryParams& p) override {
            const auto& c = std::get<ConeGeomParams>(p);
            radius_   = c.radius;
            height_   = c.height;
            segments_ = (c.segments < 3) ? 3 : c.segments;
        }

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float radius_;
        float height_;
        int   segments_;
    };

} // namespace YoRigine
