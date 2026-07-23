#pragma once
// ===========================================================
// SphereGeometry.h
//
// UV 球メッシュを生成するジオメトリ。
// スモーク球・エネルギー球・オーラなど球状エフェクト向け。
// 姿勢（位置・スケール）が変わった時だけ頂点を再構築する。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class SphereGeometry : public VfxGeometry
    {
    public:
        explicit SphereGeometry(const SphereGeomParams& params);

        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        /// 位置またはスケールが変わった時だけ再構築する
        bool IsDirty(const VfxGeomState& prev,
                     const VfxGeomState& curr) const override;

        void ApplyParams(const VfxGeometryParams& p) override {
            const auto& s = std::get<SphereGeomParams>(p);
            radius_  = s.radius;
            rings_   = (s.rings   < 3) ? 3 : s.rings;
            sectors_ = (s.sectors < 3) ? 3 : s.sectors;
        }

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float radius_;
        int   rings_;
        int   sectors_;
    };

} // namespace YoRigine
