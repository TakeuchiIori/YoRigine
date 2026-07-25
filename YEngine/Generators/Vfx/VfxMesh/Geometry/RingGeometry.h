#pragma once
// ===========================================================
// RingGeometry.h
//
// カメラを向くリング（アニュラス）を生成するジオメトリ。
// 衝撃波・魔法陣外枠・エフェクトリングなど向け。
// カメラ方向が変わるため毎フレーム再構築する。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class RingGeometry : public VfxGeometry
    {
    public:
        explicit RingGeometry(const RingGeomParams& params);

        /// billboard=true はカメラ向き、false は rotation の平面にリングを生成する
        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        bool IsDirty(const VfxGeomState&, const VfxGeomState&) const override { return true; }

        void ApplyParams(const VfxGeometryParams& p) override {
            const auto& r = std::get<RingGeomParams>(p);
            outerRadius_ = r.outerRadius;
            innerRatio_  = (r.innerRatio < 0.f) ? 0.f : (r.innerRatio > 0.99f ? 0.99f : r.innerRatio);
            segments_    = (r.segments < 3) ? 3 : r.segments;
            billboard_   = r.billboard;
        }

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float outerRadius_;
        float innerRatio_;
        int   segments_;
        bool  billboard_;
    };

} // namespace YoRigine
