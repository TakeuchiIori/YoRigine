#include "DescriptorHeap.h"
#include "DirectXCommon.h"

// C++
#include <cassert>

/// <summary>
/// DirectXCommon 参照を保持
/// </summary>
void DescriptorHeap::Initialize(YoRigine::DirectXCommon* directXCommon)
{
	directXCommon_ = directXCommon;
}

/// <summary>
/// ディスクリプタヒープを作成して返す
/// </summary>
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
DescriptorHeap::CreateDescriptorHeap(
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT numDescriptors,
	bool shaderVisible)
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;

	//-------------------- ヒープ設定 --------------------//
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = heapType;                                                // ヒープ種別
	desc.NumDescriptors = numDescriptors;                                // 確保数
	desc.Flags = shaderVisible
		? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		: D3D12_DESCRIPTOR_HEAP_FLAG_NONE;                               // SRV/UAV/CBV は可視ヒープにできる

	//-------------------- ヒープ作成 --------------------//
	HRESULT hr = directXCommon_->GetDevice()->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(&heap)
	);
	if (FAILED(hr)) {
		assert(SUCCEEDED(hr) && "DescriptorHeap の作成に失敗しました");
	}

	return heap;
}

/// <summary>
/// ディスクリプタヒープから CPU ハンドルを取得
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE
DescriptorHeap::GetCPUDescriptorHandle(
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	uint32_t descriptorSize,
	uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
	return handle;
}

/// <summary>
/// ディスクリプタヒープから GPU ハンドルを取得
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE
DescriptorHeap::GetGPUDescriptorHandle(
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	uint32_t descriptorSize,
	uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<UINT64>(descriptorSize) * index;
	return handle;
}
