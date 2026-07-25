#pragma once
// ===========================================================
// ProceduralMeshBase.h
//
// VfxMesh 系の全メッシュクラスが継承する基底クラス。
// 頂点バッファの管理（DynamicVertexBuffer）と、
// Spawner / Editor が型を知らずに操作するための
// レンダリングインターフェースを提供する。
//
// 継承クラスが実装するもの:
//   Update()      - 毎フレームの頂点再構築
//   Drive()       - VfxEvalState から姿勢・パラメータを受け取る
//   Draw()        - 頂点バッファのバインドと DrawInstanced
//   GetPSOName()  - 使用する PSO の名前（YPipelineManager のキー）
//   GetCBByteSize() / FillCB() - 定数バッファの管理
// ===========================================================
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <Buffers/DynamicVertexBuffer.h>
#include "MathFunc.h"
#include "VfxEvalState.h"
#include "VfxEffectAsset.h"

namespace YoRigine {

    class DirectXCommon;

    // ===========================================================
    // 共通頂点レイアウト（VfxMesh_Common.hlsli の INPUT と一致）
    // ===========================================================
    struct ProceduralMeshVertex
    {
        Vector3 position;   // POSITION  - ワールド座標
        Vector2 texcoord;   // TEXCOORD0 - UV（u=始端→終端の正規化位置、v=リボン左0/右1）
        Vector4 color;      // COLOR0    - 頂点色（r を本線/枝フラグに流用する場合あり）
        float   age;        // TEXCOORD1 - 0..1 の正規化寿命（シェーダのフェードに使う）
    };

    // ===========================================================
    // ProceduralMeshBase
    // ===========================================================
    class ProceduralMeshBase
    {
    public:
        ProceduralMeshBase() = default;
        virtual ~ProceduralMeshBase() = default;

        ProceduralMeshBase(const ProceduralMeshBase&) = delete;
        ProceduralMeshBase& operator=(const ProceduralMeshBase&) = delete;

        // ── 継承クラスが実装する純粋仮想インターフェース ──────

        /// 毎フレーム呼ばれる。頂点の再構築とアップロードを行う
        virtual void Update(float deltaTime) = 0;

        /// VfxEvalState（位置・スケール・進捗など）を受け取り自分の姿勢に反映する。
        /// 動きを持たないメッシュはオーバーライド不要（デフォルトは何もしない）
        virtual void Drive(const VfxEvalState& /*state*/) {}

        /// 頂点バッファをバインドして描画コールを発行する
        virtual void Draw(ID3D12GraphicsCommandList* cmdList) = 0;

        // ── レンダリングインターフェース ──────────────────────
        // Spawner / Editor がメッシュの具体型を知らずに描画できるよう
        // PSO 名・CB サイズ・CB 書き込みを仮想関数で提供する。

        /// 使用する PSO の名前（YPipelineManager::GetParameterIndices のキー）
        virtual const char* GetPSOName()    const { return ""; }

        /// 定数バッファのバイトサイズ（256 バイトアライメント済みの値を返す）
        virtual size_t      GetCBByteSize() const { return 0; }

        /// Map 済みの void* に定数バッファの値を書き込む
        struct CBFillArgs {
            const VfxElement& def;           ///< エレメント定義（パラメータ参照用）
            const Vector4&    tint;          ///< モジュール評価結果の色乗算 (rgb) / 不透明度 (a)
            float             age;           ///< エフェクト全体の経過秒
            float             burstProgress; ///< -1=ループ継続, 0..1=ワンショット進捗
            float             beamRadiusScale;
            float             beamGlowScale;
        };
        virtual void FillCB(void* /*mapped*/, const CBFillArgs& /*args*/) const {}

        /// テクスチャ等の追加リソース(SRV)を root signature へバインドする。
        /// CBV バインドの後・Draw の前に描画側から呼ぶ。デフォルトは何もしない。
        /// idx は YPipelineManager::GetParameterIndices(GetPSOName()) の中身。
        virtual void BindResources(ID3D12GraphicsCommandList* /*cmdList*/,
                                   const std::unordered_map<std::string, UINT>& /*idx*/) const {}

        /// マテリアルの「種別インデックス」を後から差し替える（量産時の見た目バリエーション用）。
        /// 各マテリアルは自分が使う引数だけ拾い、他は無視する（規約: AreaField=(a:魔法陣, b:内部質感),
        /// RimFx=(c:縁スタイル)）。デフォルトは何もしない。
        virtual void SetStyleIndices(int /*a*/, int /*b*/, int /*c*/) {}

        // ── 状態アクセサ ─────────────────────────────────────

        /// 描画を行うかどうか
        bool     IsVisible()    const { return isVisible_; }
        void     SetVisible(bool v)   { isVisible_ = v; }

        /// 現在アップロード済みの頂点数
        uint32_t GetVertexCount() const { return vertexBuffer_.GetActiveVertexCount(); }

    protected:
        // ── 継承クラス向けヘルパー ────────────────────────────

        /// 頂点バッファを initialCapacity 分確保する（Initialize 内で1回だけ呼ぶ）
        void InitBuffer(size_t initialCapacity)
        {
            vertexBuffer_.Initialize(initialCapacity, sizeof(ProceduralMeshVertex));
        }

        /// verts を GPU 頂点バッファにアップロードして描画可能にする
        uint32_t UploadVertices(const std::vector<ProceduralMeshVertex>& verts)
        {
            return vertexBuffer_.Upload(verts);
        }

        /// コマンドリストに頂点バッファをバインドする
        void BindVertexBuffer(ID3D12GraphicsCommandList* cmdList) const
        {
            const auto& vbv = vertexBuffer_.GetView();
            cmdList->IASetVertexBuffers(0, 1, &vbv);
        }

    protected:
        DynamicVertexBuffer vertexBuffer_;
        bool                isVisible_ = true;
    };

} // namespace YoRigine
