#include "RtvManager.h"

// Engine
#include <DeviceManager.h>
#include <SwapChainManager.h>
#include <SrvManager.h>

// C++
#include "d3dx12.h"
#include <cassert>

/// <summary>
/// RTV マネージャーの初期化（ディスクリプタヒープ作成）
/// </summary>
void RtvManager::Initialize(DeviceManager* deviceManager, uint32_t maxCount)
{
	deviceManager_ = deviceManager;
	maxCount_ = maxCount;

	CreateHeap(maxCount);
}

/// <summary>
/// 使っているリソースの破棄
/// </summary>
void RtvManager::Finalize()
{
	renderTargets_.clear();
	nameToIndex_.clear();
	heap_.Reset();
}

/// <summary>
/// 指定サイズの RenderTarget を新規作成して登録
/// </summary>
uint32_t RtvManager::Create(
	const std::string& name,
	uint32_t width,
	uint32_t height,
	DXGI_FORMAT format,
	const Vector4& clearColor,
	bool createSRV)
{
	// 同名チェック
	assert(nameToIndex_.find(name) == nameToIndex_.end());
	assert(currentIndex_ < maxCount_);

	//-------------------- 情報準備 --------------------//
	auto rt = std::make_unique<RenderTarget>();
	rt->name = name;
	rt->width = width;
	rt->height = height;
	rt->format = format;

	// クリア値
	rt->clearValue.Format = format;
	rt->clearValue.Color[0] = clearColor.x;
	rt->clearValue.Color[1] = clearColor.y;
	rt->clearValue.Color[2] = clearColor.z;
	rt->clearValue.Color[3] = clearColor.w;

	//-------------------- リソース作成 --------------------//
	rt->resource = CreateRenderTextureResource(width, height, rt->clearValue);

	//-------------------- RTV 作成 --------------------//
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	rt->rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		heap_->GetCPUDescriptorHandleForHeapStart(),
		currentIndex_,
		descriptorSize_
	);

	deviceManager_->GetDevice()->CreateRenderTargetView(
		rt->resource.Get(),
		&rtvDesc,
		rt->rtvHandle
	);

	//-------------------- SRV 作成（必要な場合） --------------------//
	if (createSRV) {
		rt->srvIndex = SrvManager::GetInstance()->Allocate();

		SrvManager::GetInstance()->CreateSRVforRenderTexture(
			rt->srvIndex,
			rt->resource.Get()
		);

		rt->srvHandleCPU = SrvManager::GetInstance()->GetCPUDescriptorHandle(rt->srvIndex);
		rt->srvHandleGPU = SrvManager::GetInstance()->GetGPUDescriptorHandle(rt->srvIndex);
	}

	//-------------------- 登録 --------------------//
	uint32_t index = static_cast<uint32_t>(renderTargets_.size());
	renderTargets_.push_back(std::move(rt));
	nameToIndex_[name] = index;
	currentIndex_++;

	return index;
}

/// <summary>
/// 既存リソースを RTV として登録
/// </summary>
uint32_t RtvManager::Register(
	const std::string& name,
	Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	DXGI_FORMAT format,
	const Vector4& clearColor)
{
	assert(nameToIndex_.find(name) == nameToIndex_.end());
	assert(currentIndex_ < maxCount_);
	assert(resource != nullptr);

	//-------------------- 情報作成 --------------------//
	auto rt = std::make_unique<RenderTarget>();
	rt->name = name;
	rt->resource = resource;
	rt->format = format;

	// サイズ取得
	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	rt->width = static_cast<uint32_t>(desc.Width);
	rt->height = static_cast<uint32_t>(desc.Height);

	// クリア値
	rt->clearValue.Format = format;
	rt->clearValue.Color[0] = clearColor.x;
	rt->clearValue.Color[1] = clearColor.y;
	rt->clearValue.Color[2] = clearColor.z;
	rt->clearValue.Color[3] = clearColor.w;

	//-------------------- RTV 作成 --------------------//
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	rt->rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		heap_->GetCPUDescriptorHandleForHeapStart(),
		currentIndex_,
		descriptorSize_
	);

	deviceManager_->GetDevice()->CreateRenderTargetView(
		rt->resource.Get(),
		&rtvDesc,
		rt->rtvHandle
	);

	//-------------------- 登録 --------------------//
	uint32_t index = static_cast<uint32_t>(renderTargets_.size());
	renderTargets_.push_back(std::move(rt));
	nameToIndex_[name] = index;
	currentIndex_++;

	return index;
}

/// <summary>
/// 指定 RenderTarget をクリア
/// </summary>
void RtvManager::Clear(uint32_t index, ID3D12GraphicsCommandList* commandList)
{
	assert(index < renderTargets_.size());

	const auto& rt = renderTargets_[index];
	commandList->ClearRenderTargetView(rt->rtvHandle, rt->clearValue.Color, 0, nullptr);
}

/// <summary>
/// 名前指定でクリア
/// </summary>
void RtvManager::Clear(const std::string& name, ID3D12GraphicsCommandList* commandList)
{
	auto it = nameToIndex_.find(name);
	assert(it != nameToIndex_.end());

	Clear(it->second, commandList);
}

/// <summary>
/// リソース遷移バリア
/// </summary>
void RtvManager::TransitionResource(
	ID3D12GraphicsCommandList* commandList,
	uint32_t index,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after)
{
	assert(index < renderTargets_.size());

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		renderTargets_[index]->resource.Get(),
		before,
		after
	);

	commandList->ResourceBarrier(1, &barrier);
}

/// <summary>
/// 名前指定でリソース遷移
/// </summary>
void RtvManager::TransitionBarrier(
	ID3D12GraphicsCommandList* commandList,
	const std::string& name,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after)
{
	auto it = nameToIndex_.find(name);
	assert(it != nameToIndex_.end());

	TransitionResource(commandList, it->second, before, after);
}

/// <summary>
/// RenderTargetのセット（インデックス指定）
/// </summary>
void RtvManager::SetRenderTargets(
	ID3D12GraphicsCommandList* commandList,
	const std::vector<uint32_t>& indices,
	const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle)
{
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
	rtvHandles.reserve(indices.size());

	for (uint32_t index : indices) {
		assert(index < renderTargets_.size());
		rtvHandles.push_back(renderTargets_[index]->rtvHandle);
	}

	commandList->OMSetRenderTargets(
		static_cast<UINT>(rtvHandles.size()),
		rtvHandles.data(),
		FALSE,
		dsvHandle
	);
}

/// <summary>
/// RenderTargetのセット（名前指定）
/// </summary>
void RtvManager::SetRenderTargets(
	ID3D12GraphicsCommandList* commandList,
	const std::vector<std::string>& names,
	const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle)
{
	std::vector<uint32_t> indices;
	indices.reserve(names.size());

	for (auto& name : names) {
		auto it = nameToIndex_.find(name);
		assert(it != nameToIndex_.end());
		indices.push_back(it->second);
	}

	SetRenderTargets(commandList, indices, dsvHandle);
}

/// <summary>
/// インデックス指定で RT 情報取得
/// </summary>
const RtvManager::RenderTarget* RtvManager::Get(uint32_t index) const
{
	if (index >= renderTargets_.size()) return nullptr;
	return renderTargets_[index].get();
}

/// <summary>
/// 名前指定で RT 情報取得
/// </summary>
const RtvManager::RenderTarget* RtvManager::Get(const std::string& name) const
{
	auto it = nameToIndex_.find(name);
	if (it == nameToIndex_.end()) return nullptr;

	return renderTargets_[it->second].get();
}

/// <summary>
/// 名前 → インデックス取得
/// </summary>
uint32_t RtvManager::GetIndex(const std::string& name) const
{
	auto it = nameToIndex_.find(name);
	if (it == nameToIndex_.end()) return UINT32_MAX;

	return it->second;
}

/// <summary>
/// RTV ディスクリプタヒープ作成
/// </summary>
void RtvManager::CreateHeap(uint32_t maxCount)
{
	auto device = deviceManager_->GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = maxCount;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap_));
	assert(SUCCEEDED(hr));
	(void)hr;

	descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

/// <summary>
/// RenderTarget用のTexture2D リソース作成
/// </summary>
Microsoft::WRL::ComPtr<ID3D12Resource>
RtvManager::CreateRenderTextureResource(uint32_t width, uint32_t height, const D3D12_CLEAR_VALUE& clearValue)
{
	D3D12_RESOURCE_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.DepthOrArraySize = 1;
	desc.Format = clearValue.Format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	HRESULT hr = deviceManager_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&clearValue,
		IID_PPV_ARGS(&resource)
	);

	assert(SUCCEEDED(hr));
	(void)hr;
	return resource;
}
