#pragma once

// C++
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

class DeviceManager;
class SrvManager;
/// <summary>
/// DSV管理クラス
/// </summary>
class DsvManager
{
public:
	// 深度ステンシル情報
	struct DepthStencil
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		uint32_t srvIndex = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
		std::string name;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t arraySize = 1;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		float clearDepth = 1.0f;
		uint8_t clearStencil = 0;
		// Texture2DArray のスライスごとの DSV（arraySize > 1 のとき使用。[0] は dsvHandle と同一）
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> sliceDsvHandles;
	};
	// シャドウカスケード（CSM）の枚数
	static const int32_t kShadowCascadeCount = 3;
	// 1 カスケードあたりの解像度
	static const int32_t kShadowmapWidth = 2048;
	static const int32_t kShadowmapHeight = 2048;
public:
	///************************* 基本関数 *************************///

	void Initialize(DeviceManager* deviceManager, uint32_t maxCount = 8);
	void Finalize();

	// 深度ステンシルバッファの作成
	uint32_t Create(
		const std::string& name,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		bool createSRV = true,
		float clearDepth = 1.0f,
		uint8_t clearStencil = 0
	);

	// Texture2DArray 深度バッファの作成（シャドウカスケード用）。
	// スライスごとの DSV と、配列全体を読む SRV を作る。
	uint32_t CreateArray(
		const std::string& name,
		uint32_t width,
		uint32_t height,
		uint32_t arraySize,
		DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT,
		bool createSRV = true,
		float clearDepth = 1.0f,
		uint8_t clearStencil = 0
	);

	// クリア
	void Clear(
		uint32_t index,
		ID3D12GraphicsCommandList* commandList,
		D3D12_CLEAR_FLAGS flags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL
	);
	void Clear(
		const std::string& name,
		ID3D12GraphicsCommandList* commandList,
		D3D12_CLEAR_FLAGS flags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL
	);
	// Texture2DArray の特定スライスをクリア
	void ClearSlice(
		const std::string& name,
		uint32_t slice,
		ID3D12GraphicsCommandList* commandList,
		D3D12_CLEAR_FLAGS flags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL
	);

	// リソースバリア
	void TransitionResource(
		ID3D12GraphicsCommandList* commandList,
		uint32_t index,
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after
	);
	void TransitionBarrier(
		ID3D12GraphicsCommandList* commandList,
		const std::string& name,
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after
	);

public:
	///************************* アクセッサ *************************///

	const DepthStencil* Get(uint32_t index) const;
	const DepthStencil* Get(const std::string& name) const;
	uint32_t GetIndex(const std::string& name) const;

	// DSVハンドルの取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(uint32_t index) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(const std::string& name) const;
	// Texture2DArray のスライス DSV ハンドルの取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetSliceHandle(const std::string& name, uint32_t slice) const;

	ID3D12DescriptorHeap* GetHeap() const { return heap_.Get(); }
	uint32_t GetDescriptorSize() const { return descriptorSize_; }

private:
	///************************* 内部処理 *************************///

	// ディスクリプタヒープの作成
	void CreateHeap(uint32_t maxCount);

	// 深度ステンシルリソースの作成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t arraySize = 1);

private:
	///************************* メンバ変数 *************************///

	DeviceManager* deviceManager_ = nullptr;

	// ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
	uint32_t descriptorSize_ = 0;
	uint32_t maxCount_ = 0;
	uint32_t currentIndex_ = 0;

	// 深度ステンシル管理
	std::vector<std::unique_ptr<DepthStencil>> depthStencils_;
	std::unordered_map<std::string, uint32_t> nameToIndex_;
};