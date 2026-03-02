#pragma once

// C++
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXTex.h>

namespace YoRigine {
	class DirectXCommon;
}
class SrvManager
{
public:
	///************************* 基本関数 *************************///

	// 最大SRV数
	static const uint32_t kMaxSRVCount_;
	static SrvManager* GetInstance();
	void Initialize(YoRigine::DirectXCommon* dxCommon);
	void Finalize();
	SrvManager() = default;
	~SrvManager() = default;

	// アロケータ
	uint32_t Allocate();
	uint32_t Allocate(uint32_t count);

	// 描画の前準備
	void PreDraw();

	// SRVセット
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	// 確保可能チェック
	bool IsAllocation();

	// SRV生成（テクスチャ用）
	// SRV生成（Structured Buffer用)
	// SRV生成（RenderTexture用）
	// SRV生成（Depth用）
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DirectX::TexMetadata metadata);
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	void CreateSRVforRenderTexture(uint32_t srvIndex, ID3D12Resource* pResource);
	void CreateSRVforDepth(uint32_t srvIndex, ID3D12Resource* pResource);

	// UAV生成 (StructureBuffer用)
	void CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);


public:
	///************************* アクセッサ *************************///

	// SRVの指定番号のCPUディスクリプタハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	// SRVの指定番号のGPUディスクリプタハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	// SRV用のディスクリプタヒープを取得
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }


private:
	///************************* メンバ変数 *************************///

	static SrvManager* instance;
	SrvManager(SrvManager&) = delete;
	SrvManager& operator = (SrvManager&) = delete;

	// ポインタ
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;
	// ディスクリプタサイズ
	uint32_t descriptorSize_ = 0;
	// デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
};

