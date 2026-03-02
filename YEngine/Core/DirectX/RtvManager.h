#pragma once

// C++
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <cassert>

// Engine
#include "Vector4.h"

class DeviceManager;
class SrvManager;
/// <summary>
/// RTVの管理クラス
/// </summary>
class RtvManager
{
public:
	// レンダーターゲット情報
	struct RenderTarget
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
		uint32_t srvIndex = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
		D3D12_CLEAR_VALUE clearValue;
		std::string name;
		uint32_t width = 0;
		uint32_t height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};

public:
	///************************* 基本関数 *************************///

	void Initialize(DeviceManager* deviceManager, uint32_t maxCount = 16);
	void Finalize();

	// レンダーターゲットの作成
	uint32_t Create(
		const std::string& name,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		const Vector4& clearColor = { 0.1f, 0.1f, 0.2f, 1.0f },
		bool createSRV = true
	);

	// 既存リソースの登録（スワップチェーン用）
	uint32_t Register(
		const std::string& name,
		Microsoft::WRL::ComPtr<ID3D12Resource> resource,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		const Vector4& clearColor = { 0.1f, 0.25f, 0.5f, 1.0f }
	);

	// クリア
	void Clear(uint32_t index, ID3D12GraphicsCommandList* commandList);
	void Clear(const std::string& name, ID3D12GraphicsCommandList* commandList);

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

	// レンダーターゲットのセット
	void SetRenderTargets(
		ID3D12GraphicsCommandList* commandList,
		const std::vector<uint32_t>& indices,
		const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = nullptr
	);

	void SetRenderTargets(
		ID3D12GraphicsCommandList* commandList,
		const std::vector<std::string>& names,
		const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = nullptr
	);

public:
	///************************* アクセッサ *************************///
	const RenderTarget* Get(uint32_t index) const;
	const RenderTarget* Get(const std::string& name) const;

	uint32_t GetIndex(const std::string& name) const;

	ID3D12DescriptorHeap* GetHeap() const { return heap_.Get(); }
	uint32_t GetDescriptorSize() const { return descriptorSize_; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(uint32_t index) const
	{
		assert(index < renderTargets_.size());
		return renderTargets_[index]->rtvHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(const std::string& name) const
	{
		auto it = nameToIndex_.find(name);
		assert(it != nameToIndex_.end());
		return GetHandle(it->second);
	}

private:
	///************************* 内部処理 *************************///

	// ディスクリプタヒープの作成
	void CreateHeap(uint32_t maxCount);

	// レンダーテクスチャリソースの作成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
		uint32_t width,
		uint32_t height,
		const D3D12_CLEAR_VALUE& clearValue
	);

private:
	DeviceManager* deviceManager_ = nullptr;

	// ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
	uint32_t descriptorSize_ = 0;
	uint32_t maxCount_ = 0;
	uint32_t currentIndex_ = 0;

	// レンダーターゲット管理
	std::vector<std::unique_ptr<RenderTarget>> renderTargets_;
	std::unordered_map<std::string, uint32_t> nameToIndex_;
};