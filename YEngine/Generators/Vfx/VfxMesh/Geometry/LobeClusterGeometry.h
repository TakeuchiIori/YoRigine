#pragma once
// ===========================================================
// LobeClusterGeometry.h
//
// 小球(塊=lobe)を多数寄せ集めて「もくもく」した塊感を作るジオメトリ。
// 爆発煙・キノコ雲の頭/柱に使う。Sphere が1個のツルッとした球なのに対し、
// これは lobeCount 個の塊でシルエットをボコボコにする。
//   - lobeCount を増やす＝もくもくの数が増える
//   - age(progress) 駆動で塊が時間差(stagger)にポップ膨張する
// 配置は seed による決定論的乱数で固定し、フレーム間でチラつかせない。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxGeometry.h>
#include <Vfx/VfxMesh/Core/VfxGeometryTypes.h>

namespace YoRigine {

    class LobeClusterGeometry : public VfxGeometry
    {
    public:
        explicit LobeClusterGeometry(const LobeClusterGeomParams& params);

        void Build(std::vector<ProceduralMeshVertex>& out,
                   const VfxGeomState& state) override;

        // 位置・スケール・進行(progress) いずれかが変わったら再構築する。
        // ポップ膨張は progress で進むので、バースト中は毎フレーム再構築される。
        bool IsDirty(const VfxGeomState& prev,
                     const VfxGeomState& curr) const override;

        void ApplyParams(const VfxGeometryParams& p) override;

        /// GeometryRegistry に渡す記述子を返す
        static VfxGeometryDesc Describe();

    private:
        float    radius_;
        int      lobeCount_;
        float    lobeRadius_;
        float    lobeJitter_;
        float    stagger_;
        int      rings_;
        int      sectors_;
        uint32_t seed_;
    };

} // namespace YoRigine
