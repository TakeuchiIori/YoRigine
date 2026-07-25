#pragma once
// ===========================================================
// VfxMeshElement.h
//
// VfxGeometry と VfxMaterial を組み合わせる汎用メッシュ要素。
// ProceduralMeshBase を継承するので VfxMeshSpawner がそのまま管理できる。
//
// 新エフェクトの追加（VfxElementDesc::create の lambda 内）:
//   auto elem = std::make_unique<VfxMeshElement>(
//       std::make_unique<ConeGeometry>(param.radius, param.height),
//       std::make_unique<NoiseMaterial>(param));
//   elem->SetCamera(cam);
//   return elem;
// ===========================================================
#include "ProceduralMeshBase.h"
#include "VfxGeometry.h"
#include "VfxMaterial.h"
#include "VfxGeometryTypes.h"
#include "VfxMaterialTypes.h"
#include <memory>
#include <vector>

namespace YoRigine { class Camera; }

namespace YoRigine {

    class VfxMeshElement : public ProceduralMeshBase
    {
    public:
        VfxMeshElement(std::unique_ptr<VfxGeometry> geometry,
                       std::unique_ptr<VfxMaterial> material);

        /// 型 ID + パラメータから2軸合成でエレメントを生成する（レジストリ経由）。
        /// どちらかの生成に失敗したら nullptr を返す。
        static std::unique_ptr<VfxMeshElement> CreateComposed(
            VfxGeometryType   geomType, const VfxGeometryParams& geomParams,
            VfxMaterialType   matType,  const VfxMaterialParams& matParams,
            YoRigine::Camera*           camera);

        // ── 設定 ─────────────────────────────────────────────

        /// ビルボード形状などカメラが必要な Geometry に渡す
        void SetCamera(YoRigine::Camera* camera) { geomState_.camera = camera; }

        /// Geometry / Material のパラメータを差し替える（エディタのライブ編集用）。
        /// 次の Update で必ずジオメトリを再構築する。
        void ApplyParams(const VfxGeometryParams& geom, const VfxMaterialParams& mat) {
            geometry_->ApplyParams(geom);
            material_->ApplyParams(mat);
            forceRebuild_ = true;
        }

        // ── 毎フレーム ────────────────────────────────────────

        /// VfxEvalState から position / scale / progress を geomState_ に反映する
        void Drive(const VfxEvalState& state) override;

        /// IsDirty なら Geometry を再構築してアップロードする
        void Update(float deltaTime) override;

        /// 頂点バッファをバインドして描画する
        void Draw(ID3D12GraphicsCommandList* cmdList) override;

        // ── レンダリングインターフェース ──────────────────────

        const char* GetPSOName()    const override { return material_->GetPSOName(); }
        size_t      GetCBByteSize() const override { return material_->GetCBByteSize(); }

        /// マテリアルの追加リソース（テクスチャ SRV 等）をバインドする。
        /// CBV バインド後・Draw 前に描画側から呼ぶ。
        void BindResources(ID3D12GraphicsCommandList* cmdList,
                           const std::unordered_map<std::string, UINT>& idx) const override {
            material_->BindResources(cmdList, idx);
        }

        /// マテリアルの種別インデックス差し替えを委譲する
        void SetStyleIndices(int a, int b, int c) override {
            material_->SetStyleIndices(a, b, c);
        }

        /// CBFillArgs を VfxMatFillArgs に変換してマテリアルに委譲する
        void        FillCB(void* mapped, const CBFillArgs& args) const override;

    private:
        std::unique_ptr<VfxGeometry> geometry_;
        std::unique_ptr<VfxMaterial> material_;

        VfxGeomState prevGeomState_; ///< IsDirty の比較用（前フレームの状態）
        VfxGeomState geomState_;     ///< 現在のフレームの状態

        std::vector<ProceduralMeshVertex> vertices_;
        float time_ = 0.f;
        bool  forceRebuild_ = false; ///< ApplyParams 後に次の Update で強制再構築
    };

} // namespace YoRigine
