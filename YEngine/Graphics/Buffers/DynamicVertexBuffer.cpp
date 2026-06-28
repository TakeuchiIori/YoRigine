#include "DynamicVertexBuffer.h"
#include "DirectXCommon.h"

#include <stdexcept>

//----------------------------------------------------------------------
// 初期化 / 解放
//----------------------------------------------------------------------

void DynamicVertexBuffer::Initialize(size_t initialCapacity, size_t stride) {
    assert(initialCapacity > 0 && "initialCapacity must be > 0");
    assert(stride > 0 && "stride must be > 0");

    dxCommon_ = YoRigine::DirectXCommon::GetInstance();
    stride_ = stride;

    // 最初のバッファを確保 + Persistent Map
    Resize(initialCapacity);
}

void DynamicVertexBuffer::Finalize() {
    if (mappedPtr_ && buffer_) {
        buffer_->Unmap(0, nullptr);
        mappedPtr_ = nullptr;
    }
    buffer_.Reset();
    capacity_ = 0;
    activeVertexCount_ = 0;
    vbv_ = {};
}

//----------------------------------------------------------------------
// 内部: リサイズ
//----------------------------------------------------------------------

void DynamicVertexBuffer::Resize(size_t newCapacity) {
    // 既存バッファが Persistent Map 済みならアンマップしてから破棄
    if (mappedPtr_ && buffer_) {
        buffer_->Unmap(0, nullptr);
        mappedPtr_ = nullptr;
    }
    buffer_.Reset();
    const size_t byteSize = newCapacity * stride_;
    buffer_ = dxCommon_->CreateBufferResource(byteSize);

    if (!buffer_) {
        throw std::runtime_error("DynamicVertexBuffer: CreateBufferResource failed");
    }

    const HRESULT hr = buffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPtr_));
    if (FAILED(hr)) {
        throw std::runtime_error("DynamicVertexBuffer: Map failed");
    }

    capacity_ = newCapacity;

    // VBV を新しいバッファに向け直す (activeVertexCount_ は Upload 側で更新)
    vbv_.BufferLocation = buffer_->GetGPUVirtualAddress();
    vbv_.StrideInBytes = static_cast<UINT>(stride_);
    vbv_.SizeInBytes = 0; // Upload 後に UpdateView() で確定させる
}

//----------------------------------------------------------------------
// 内部: VBV 更新
//----------------------------------------------------------------------

void DynamicVertexBuffer::UpdateView() {
    vbv_.BufferLocation = buffer_->GetGPUVirtualAddress();
    vbv_.StrideInBytes = static_cast<UINT>(stride_);
    vbv_.SizeInBytes = activeVertexCount_ * static_cast<UINT>(stride_);
}