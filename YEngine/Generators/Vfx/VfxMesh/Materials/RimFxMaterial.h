#pragma once
// ===========================================================
// RimFxMaterial.h
//
// 縁の縦演出シェーダ（"VfxMeshRimFx" PSO）を使うマテリアル。
// RimCurtain（縦スカート）と組み、v(下0→上1)に沿って上へ立ち上がる表現を描く。
// style で 炎/霊気/電撃/… を切り替え、1マテリアルで量産する（膨張は element scale）。
//
// パラメータは VfxMaterialParams.h の RimFxMatParams を使う。
// ===========================================================
#include <Vfx/VfxMesh/Core/VfxMaterial.h>
#include <Vfx/VfxMesh/Core/VfxMaterialTypes.h>
#include <string>

namespace YoRigine {

    // シェーダ b1: gMeshParam（VfxMesh_Common.hlsli の RimFxParams と一致させること）
    struct RimFxParamsCB
    {
        Vector4 color;      // Row0: 根本の色(HDR) / a=強度
        Vector4 tipColor;   // Row1: 先端(上)の色(HDR)
        float   style;      // Row2: 表現種別（int を float 化）
        float   riseSpeed;
        float   noiseScale;
        float   turbulence;
        float   time;       // Row3
        float   texStrength; // テクスチャの寄与(0=無効)
        float   texTiling;   // UV タイリング
        float   texScroll;   // テクスチャ縦スクロール速度
    };

    class RimFxMaterial : public VfxMaterial
    {
    public:
        explicit RimFxMaterial(const RimFxMatParams& param);

        const char* GetPSOName()    const override { return "VfxMeshRimFx"; }
        size_t      GetCBByteSize() const override;
        void        FillCB(void* mapped, const VfxMatFillArgs& args) const override;

        void ApplyParams(const VfxMaterialParams& p) override {
            param_ = std::get<RimFxMatParams>(p);
            EnsureTexture();
        }

        // c=縁スタイル(0..7)。a,b(AreaField用)は使わない。
        void SetStyleIndices(int /*a*/, int /*b*/, int c) override {
            param_.style = c;
        }

        /// t0: gTexNoise（テクスチャ or 白フォールバック）をバインドする
        void BindResources(ID3D12GraphicsCommandList* cmdList,
                           const std::unordered_map<std::string, UINT>& idx) const override;

        /// MaterialRegistry 用の記述子を返す
        static VfxMaterialDesc Describe();

    private:
        /// texturePath が変わっていたら TextureManager に読み込ませる（白も一度確保）
        void EnsureTexture();

        RimFxMatParams param_;
        std::string    loadedTexPath_; ///< 読み込み済みパス（重複ロード防止）
    };

} // namespace YoRigine
