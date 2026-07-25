#pragma once
// ===========================================================
// DiscGeometry.h
//
// 塗りつぶし円（トライアングルファン）を生成するジオメトリ。
// 既定は地面(rotation の平面)に寝かせる = バフエリア/AoE/射程マーカー向け。
// UV は中心(0.5,0.5)→縁で length(uv*2-1)=1 になる放射UV。
// マテリアル側（AreaField 等）が texcoord から半径・角度を復元して内部を描く。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class DiscGeometry : public VfxGeometry
    {
    public:
        explicit DiscGeometry(const DiscGeomParams& params);

        /// billboard=false は rotation の平面（既定=XZ水平の地面）、true はカメラ向き
        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        bool IsDirty(const VfxGeomState&, const VfxGeomState&) const override { return true; }

        void ApplyParams(const VfxGeometryParams& p) override {
            const auto& d = std::get<DiscGeomParams>(p);
            radius_    = d.radius;
            segments_  = (d.segments < 3) ? 3 : d.segments;
            billboard_ = d.billboard;
            pitchDeg_  = d.pitchDeg;
            yawDeg_    = d.yawDeg;
        }

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float radius_;
        int   segments_;
        bool  billboard_;
        float pitchDeg_;
        float yawDeg_;
    };

} // namespace YoRigine
