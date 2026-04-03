#pragma once
// ===========================================================
// ProceduralMeshBase.h
//
// TrailMesh / LightVolumeMesh の共通基底クラス
// DynamicVertexBuffer + PSO の保持と描画コマンド発行を担う
// ===========================================================
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Buffers/DynamicVertexBuffer.h"
#include "MathFunc.h"

namespace YoRigine {

class DirectXCommon;

// -----------------------------------------------------------
// 共通頂点レイアウト  (VfxMesh_Common.hlsli に対応)
// -----------------------------------------------------------
struct ProceduralMeshVertex
{
    Vector3 position;   // POSITION
    Vector2 texcoord;   // TEXCOORD0
    Vector4 color;      // COLOR0   (rgba, alpha はフェード済み)
    float   age;        // TEXCOORD1 (0=新しい, 1=古い/端)
};

// -----------------------------------------------------------
// PSO 種別 (VfxEditor の BlendMode と対応させる)
// -----------------------------------------------------------
enum class VfxPsoType : uint8_t
{
    TrailAdd = 0,   // Trail 加算
    TrailAlpha,     // Trail 通常アルファ
    LightVolume,    // LightVolume 加算
};

// -----------------------------------------------------------
// 基底クラス
// -----------------------------------------------------------
class ProceduralMeshBase
{
public:
    ProceduralMeshBase() = default;
    virtual ~ProceduralMeshBase() = default;

    // コピー禁止
    ProceduralMeshBase(const ProceduralMeshBase&)            = delete;
    ProceduralMeshBase& operator=(const ProceduralMeshBase&) = delete;

    // --------------------------------------------------------
    // 継承クラスが実装するインターフェース
    // --------------------------------------------------------

    /// 毎フレーム頂点データを更新する (CPU 側ロジック)
    /// @param deltaTime 前フレームからの経過秒
    virtual void Update(float deltaTime) = 0;

    /// GPU バッファへ Upload → SetVB → DrawInstanced
    /// コマンドリスト記録フェーズに呼ぶ
    virtual void Draw(ID3D12GraphicsCommandList* cmdList) = 0;

    // --------------------------------------------------------
    // 共通アクセサ
    // --------------------------------------------------------
    bool IsVisible()  const { return isVisible_; }
    void SetVisible(bool v) { isVisible_ = v; }

    uint32_t GetVertexCount() const { return vertexBuffer_.GetActiveVertexCount(); }

protected:
    // --------------------------------------------------------
    // 派生クラスから使うユーティリティ
    // --------------------------------------------------------

    /// DynamicVertexBuffer を初期化する (派生クラスのInitialize内で呼ぶ)
    void InitBuffer(size_t initialCapacity)
    {
        vertexBuffer_.Initialize(initialCapacity, sizeof(ProceduralMeshVertex));
    }

    /// 頂点列を GPU へ送り VBV を更新する
    uint32_t UploadVertices(const std::vector<ProceduralMeshVertex>& verts)
    {
        return vertexBuffer_.Upload(verts);
    }

    /// VBV を CommandList にセットする
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
