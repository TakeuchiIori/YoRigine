#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <cstdint>
#include <cassert>

// 前方宣言
namespace YoRigine { class DirectXCommon; }

/// 動的頂点バッファ
/// 毎フレーム CPU から頂点データを書き換える用途に特化したバッファ
/// 容量が足りなくなったら自動で 2 倍に再確保する
class DynamicVertexBuffer
{
public:
    DynamicVertexBuffer() = default;
    ~DynamicVertexBuffer() = default;

    // コピー禁止 (GPU リソースを一意に所有)
    DynamicVertexBuffer(const DynamicVertexBuffer&) = delete;
    DynamicVertexBuffer& operator=(const DynamicVertexBuffer&) = delete;

    // ムーブは許可
    DynamicVertexBuffer(DynamicVertexBuffer&&) = default;
    DynamicVertexBuffer& operator=(DynamicVertexBuffer&&) = default;

    //------------------------------------------------------------------
    // 初期化 / 解放
    //------------------------------------------------------------------

    /// @param initialCapacity  最初に確保する頂点数
    /// @param stride           1 頂点のバイトサイズ (sizeof(ProceduralMeshVertex) など)
    void Initialize(size_t initialCapacity, size_t stride);

    /// Persistent Map を解除してリソースを解放する
    void Finalize();

    //------------------------------------------------------------------
    // 毎フレーム呼ぶ
    //------------------------------------------------------------------

    /// CPU 側の頂点データを GPU バッファへ書き込む。
    /// 容量が足りなければ自動でリサイズ（再確保）する。
    /// @return 実際に書き込んだ頂点数
    template<typename TVertex>
    uint32_t Upload(const std::vector<TVertex>& vertices) {
        if (vertices.empty()) {
            activeVertexCount_ = 0;
            return 0;
        }

        assert(sizeof(TVertex) == stride_ && "stride mismatch");

        // 容量不足なら拡張
        if (vertices.size() > capacity_) {
            Resize(vertices.size() * 2);
        }

        // Persistent Map 済みのポインタに直接 memcpy
        std::memcpy(mappedPtr_, vertices.data(), sizeof(TVertex) * vertices.size());

        activeVertexCount_ = static_cast<uint32_t>(vertices.size());
        UpdateView();
        return activeVertexCount_;
    }

    //------------------------------------------------------------------
    // 描画コマンド
    //------------------------------------------------------------------

    /// IASetVertexBuffers に渡す VBV を返す
    const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return vbv_; }

    /// 有効な頂点数 (最後の Upload で書き込んだ数)
    uint32_t GetActiveVertexCount() const { return activeVertexCount_; }

    /// 現在の最大容量 (頂点数)
    size_t GetCapacity() const { return capacity_; }

    /// 初期化済みか
    bool IsInitialized() const { return mappedPtr_ != nullptr; }

private:
    //------------------------------------------------------------------
    // 内部処理
    //------------------------------------------------------------------

    /// newCapacity 頂点分のバッファを再確保し、Persistent Map を張り直す
    void Resize(size_t newCapacity);

    /// VBV を現在の activeVertexCount_ に合わせて更新
    void UpdateView();

private:
    //------------------------------------------------------------------
    // メンバ変数
    //------------------------------------------------------------------
    YoRigine::DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;
    uint8_t* mappedPtr_ = nullptr;                          // Persistent Map の先頭ポインタ

    size_t   stride_ = 0;                                   // 1 頂点のバイトサイズ
    size_t   capacity_ = 0;                                 // 確保済み頂点数
    uint32_t activeVertexCount_ = 0;                        // 今フレームの有効頂点数

    D3D12_VERTEX_BUFFER_VIEW vbv_{};
};