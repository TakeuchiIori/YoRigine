#pragma once
// ===========================================================
// ProceduralMeshBase.h
// ===========================================================
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <Buffers/DynamicVertexBuffer.h>
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
        Vector4 color;      // COLOR0
        float   age;        // TEXCOORD1
    };

    // -----------------------------------------------------------
    // 基底クラス
    // -----------------------------------------------------------
    class ProceduralMeshBase
    {
    public:
        ProceduralMeshBase() = default;
        virtual ~ProceduralMeshBase() = default;

        ProceduralMeshBase(const ProceduralMeshBase&) = delete;
        ProceduralMeshBase& operator=(const ProceduralMeshBase&) = delete;

        // --------------------------------------------------------
        // 継承クラスが実装するインターフェース
        // --------------------------------------------------------

        virtual void Update(float deltaTime) = 0;

        /// @param cmdList          コマンドリスト
        /// @param cameraGPUAddress gCamera CBV の GPU アドレス (b0)
        ///                         他のパイプラインと同様に引数で受け取る
        virtual void Draw(ID3D12GraphicsCommandList* cmdList) = 0;

        // --------------------------------------------------------
        bool     IsVisible()    const { return isVisible_; }
        void     SetVisible(bool v) { isVisible_ = v; }
        uint32_t GetVertexCount() const { return vertexBuffer_.GetActiveVertexCount(); }

    protected:
        void InitBuffer(size_t initialCapacity)
        {
            vertexBuffer_.Initialize(initialCapacity, sizeof(ProceduralMeshVertex));
        }

        uint32_t UploadVertices(const std::vector<ProceduralMeshVertex>& verts)
        {
            return vertexBuffer_.Upload(verts);
        }

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