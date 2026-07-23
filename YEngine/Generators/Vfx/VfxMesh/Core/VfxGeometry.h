#pragma once
// ===========================================================
// VfxGeometry.h
//
// ジオメトリ（形状・頂点生成）の抽象基底クラス。
// Build() で頂点列を生成する責務のみを持ち、
// PSO 選択やシェーダ定数は VfxMaterial に委ねる。
//
// 組み合わせ例:
//   SphereGeometry + NoiseMaterial  → スモーク球 / エネルギー球
//   ConeGeometry   + NoiseMaterial  → 炎 / 竜巻下部
//   RingGeometry   + ShockwaveMaterial → 衝撃波リング
//   PlaneGeometry  + GlyphMaterial  → 魔法陣
// ===========================================================
#include <vector>
#include "MathFunc.h"
#include "Quaternion.h"
#include "ProceduralMeshBase.h"
#include "VfxGeometryParams.h" // ApplyParams(const VfxGeometryParams&) 用

class Camera;

namespace YoRigine {

    // ===========================================================
    // VfxGeomState
    // Build() に渡すフレームごとの状態
    // ===========================================================
    struct VfxGeomState
    {
        Vector3    position = { 0.f, 0.f, 0.f };
        float      scale    = 1.f;
        Quaternion rotation = Quaternion::Identity(); ///< 向き指定ジオメトリ（Cone / 非ビルボード Ring・Plane）で使用
        float      time     = 0.f;
        float      progress = -1.f;    ///< -1=ループ継続 / 0..1=ワンショット進捗
        Camera*    camera   = nullptr; ///< ビルボード形状（RingGeometry 等）で使用
    };

    // ===========================================================
    // VfxGeometry（抽象）
    // ===========================================================
    class VfxGeometry
    {
    public:
        virtual ~VfxGeometry() = default;

        /// state に基づいて頂点を生成し out に追加する（毎フレーム呼ばれる前提）
        virtual void Build(
            std::vector<ProceduralMeshVertex>& out,
            const VfxGeomState& state) = 0;

        /// 前フレームの状態と比べて再構築が必要か。
        /// false を返すと Build() をスキップして前フレームの頂点を流用できる。
        /// デフォルト: 毎フレーム再構築（ビルボード形状などは override しなくてよい）
        virtual bool IsDirty(
            const VfxGeomState& /*prev*/,
            const VfxGeomState& /*curr*/) const { return true; }

        /// パラメータを差し替える（エディタのライブ編集 / ホットリロード用）。
        /// 呼び出し側（VfxMeshElement）が次フレームの再構築を保証する。
        virtual void ApplyParams(const VfxGeometryParams& /*params*/) {}
    };

} // namespace YoRigine
