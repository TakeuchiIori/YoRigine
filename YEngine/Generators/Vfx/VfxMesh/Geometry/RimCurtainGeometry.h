#pragma once
// ===========================================================
// RimCurtainGeometry.h
//
// 円の縁に立つ「縦の帯（円筒の側面＝スカート）」を生成するジオメトリ。
// Disc と同じ半径で縁に立て、そこから上(+Y)へ立ち上がる炎・霊気・電撃などに使う。
// UV は u=円周まわり(0..1) / v=下(0)→上(1)。マテリアルが v に沿って上へ流す。
// 常に +Y 方向（カメラ非依存）。両面描画前提（NoCull）。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class RimCurtainGeometry : public VfxGeometry
    {
    public:
        explicit RimCurtainGeometry(const RimCurtainGeomParams& params);

        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        bool IsDirty(const VfxGeomState&, const VfxGeomState&) const override { return true; }

        void ApplyParams(const VfxGeometryParams& p) override {
            const auto& d = std::get<RimCurtainGeomParams>(p);
            radius_      = d.radius;
            heightRatio_ = d.heightRatio;
            segments_    = (d.segments < 3) ? 3 : d.segments;
        }

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float radius_;
        float heightRatio_;
        int   segments_;
    };

} // namespace YoRigine
