#pragma once
// ===========================================================
// AreaFieldMaterial.h
//
// 地面円フィールドシェーダ（"VfxMeshAreaField" PSO）を使うマテリアル。
// DiscGeometry（塗りつぶし円）と組み合わせて、バフエリア/AoE/射程を描く。
// 3スタイル（①魔法陣 ②エネルギー場 ③スキャン波）をブレンド係数で内包し、
// 1マテリアルで量産する。膨張は element scale（DiscGeometry）が担当。
//
// パラメータは VfxMaterialParams.h の AreaFieldMatParams を使う。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxMaterial.h>
#include <Vfx/VfxMesh/Core/VfxMaterialTypes.h>

namespace YoRigine {

    // シェーダ b1: gMeshParam（VfxMesh_Common.hlsli の AreaFieldParams と一致させること）
    struct AreaFieldParamsCB
    {
        Vector4 color;       // Row0: rgb=ベース色(HDR) / a=不透明度
        Vector4 edgeColor;   // Row1: 外周リムの HDR 色
        float   fillOpacity; // Row2
        float   edgeWidth;
        float   ringSpeed;
        float   noiseScale;
        float   scanSpeed;   // Row3
        float   runeCount;
        float   styleRune;
        float   styleEnergy;
        float   styleScan;   // Row4
        float   time;
        float   style;       // 魔法陣デザイン種別（0..7 を float 化）
        float   fillStyle;   // 内部の質感（0..5 を float 化）
    };

    class AreaFieldMaterial : public VfxMaterial
    {
    public:
        explicit AreaFieldMaterial(const AreaFieldMatParams& param);

        const char* GetPSOName()    const override { return "VfxMeshAreaField"; }
        size_t      GetCBByteSize() const override;
        void        FillCB(void* mapped, const VfxMatFillArgs& args) const override;

        void ApplyParams(const VfxMaterialParams& p) override {
            param_ = std::get<AreaFieldMatParams>(p);
        }

        // a=魔法陣スタイル(0..7) / b=内部質感(0..5)。c(縁スタイル)は使わない。
        void SetStyleIndices(int a, int b, int /*c*/) override {
            param_.style     = a;
            param_.fillStyle = b;
        }

        /// MaterialRegistry 用の記述子を返す
        static VfxMaterialDesc Describe();

    private:
        AreaFieldMatParams param_;
    };

} // namespace YoRigine
