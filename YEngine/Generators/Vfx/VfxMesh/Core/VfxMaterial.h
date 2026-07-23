#pragma once
// ===========================================================
// VfxMaterial.h
//
// マテリアル（PSO 選択・定数バッファ生成）の抽象基底クラス。
// VfxGeometry とは独立して、シェーダの設定のみを管理する。
// 同じマテリアルを異なるジオメトリに組み合わせることで
// 表現の幅を広げる（NoiseMaterial + SphereGeometry = スモーク、
//                   NoiseMaterial + ConeGeometry  = 炎、など）。
// ===========================================================
#include <cstddef>
#include <string>
#include <unordered_map>
#include <d3d12.h>
#include "MathFunc.h"
#include "VfxMaterialParams.h" // ApplyParams(const VfxMaterialParams&) 用

namespace YoRigine {

    // ===========================================================
    // VfxMatFillArgs
    // FillCB() に渡すモジュール評価結果
    // ===========================================================
    struct VfxMatFillArgs
    {
        const Vector4& tint;           ///< rgb=色乗算(HDR可) / a=不透明度乗算
        float          age;            ///< エフェクト全体の経過秒
        float          burstProgress;  ///< -1=ループ継続 / 0..1=ワンショット進捗
        float          beamRadiusScale = 1.f;
        float          beamGlowScale   = 1.f;

        // ジオメトリの現在のトランスフォーム（フレネル中心・ワールド半径の算出に使う）
        Vector3        position = { 0.f, 0.f, 0.f };
        float          scale    = 1.f;
    };

    // ===========================================================
    // VfxMaterial（抽象）
    // ===========================================================
    class VfxMaterial
    {
    public:
        virtual ~VfxMaterial() = default;

        /// 使用する PSO の名前（YPipelineManager のキー）
        virtual const char* GetPSOName() const = 0;

        /// 定数バッファのバイトサイズ（256 バイトアライメント済みの値を返す）
        virtual size_t GetCBByteSize() const = 0;

        /// Map 済みの void* に定数バッファの値を書き込む
        virtual void FillCB(void* mapped, const VfxMatFillArgs& args) const = 0;

        /// パラメータを差し替える（エディタのライブ編集 / ホットリロード用）
        virtual void ApplyParams(const VfxMaterialParams& /*params*/) {}

        /// テクスチャ等の追加リソース（SRV）を root signature にバインドする。
        /// CBV バインドの後・Draw の前に描画側から呼ばれる。デフォルトは何もしない。
        /// idx は YPipelineManager::GetParameterIndices(GetPSOName()) の中身。
        virtual void BindResources(ID3D12GraphicsCommandList* /*cmdList*/,
                                   const std::unordered_map<std::string, UINT>& /*idx*/) const {}

        /// 種別インデックスを差し替える（量産時の見た目バリエーション用）。
        /// 規約: AreaField=(a:魔法陣スタイル, b:内部質感) / RimFx=(c:縁スタイル)。使わない引数は無視。
        virtual void SetStyleIndices(int /*a*/, int /*b*/, int /*c*/) {}
    };

} // namespace YoRigine
