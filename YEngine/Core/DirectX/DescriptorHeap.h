#pragma once

// C++
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>

// Engine

namespace YoRigine {
	class DirectXCommon;
}
/// <summary>
/// ヒープ管理クラス
/// </summary>
class DescriptorHeap
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize(YoRigine::DirectXCommon* directXCommon);

	// ディスクリプターヒープの作成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	// 指定番号のCPUの取得
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);
	// 指定番号のGPUの取得
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);
private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* directXCommon_ = nullptr;

};

