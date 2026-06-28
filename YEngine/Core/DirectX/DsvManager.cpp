#include "DsvManager.h"

// Engine
#include "DeviceManager.h"
#include "SrvManager.h"

// C++
#include "d3dx12.h"
#include <cassert>

/// <summary>
/// DSV マネージャーの初期化（ヒープ作成）
/// </summary>
void DsvManager::Initialize(DeviceManager* deviceManager, uint32_t maxCount)
{
	deviceManager_ = deviceManager;
	maxCount_ = maxCount;

	CreateHeap(maxCount);
}

/// <summary>
/// 使用リソースの破棄
/// </summary>
void DsvManager::Finalize()
{
	depthStencils_.clear();
	nameToIndex_.clear();
	heap_.Reset();
}

/// <summary>
/// 深度ステンシルの作成と登録
/// </summary>
uint32_t DsvManager::Create(
	const std::string& name,
	uint32_t width,
	uint32_t height,
	DXGI_FORMAT format,
	bool createSRV,
	float clearDepth,
	uint8_t clearStencil)
{
	// 名前重複チェック
	assert(nameToIndex_.find(name) == nameToIndex_.end());
	assert(currentIndex_ < maxCount_);

	//-------------------- 情報準備 --------------------//
	auto ds = std::make_unique<DepthStencil>();
	ds->name = name;
	ds->width = width;
	ds->height = height;
	ds->format = format;
	ds->clearDepth = clearDepth;
	ds->clearStencil = clearStencil;

	//-------------------- リソース作成 --------------------//
	ds->resource = CreateDepthStencilTextureResource(width, height,format);

	//-------------------- DSV 作成 --------------------//
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	ds->dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		heap_->GetCPUDescriptorHandleForHeapStart(),
		currentIndex_,
		descriptorSize_
	);

	deviceManager_->GetDevice()->CreateDepthStencilView(
		ds->resource.Get(),
		&dsvDesc,
		ds->dsvHandle
	);

	//-------------------- SRV 作成（必要な場合） --------------------//
	if (createSRV) {
		ds->srvIndex = SrvManager::GetInstance()->Allocate();

		SrvManager::GetInstance()->CreateSRVforDepth(
			ds->srvIndex,
			ds->resource.Get()
		);

		ds->srvHandleCPU = SrvManager::GetInstance()->GetCPUDescriptorHandle(ds->srvIndex);
		ds->srvHandleGPU = SrvManager::GetInstance()->GetGPUDescriptorHandle(ds->srvIndex);
	}

	//-------------------- 管理配列に登録 --------------------//
	uint32_t index = static_cast<uint32_t>(depthStencils_.size());
	depthStencils_.push_back(std::move(ds));
	nameToIndex_[name] = index;
	currentIndex_++;

	return index;
}

/// <summary>
/// 指定インデックスの深度をクリア
/// </summary>
void DsvManager::Clear(
	uint32_t index,
	ID3D12GraphicsCommandList* commandList,
	D3D12_CLEAR_FLAGS flags)
{
	assert(index < depthStencils_.size());
	const auto& ds = depthStencils_[index];

	commandList->ClearDepthStencilView(
		ds->dsvHandle,
		flags,
		ds->clearDepth,
		ds->clearStencil,
		0,
		nullptr
	);
}

/// <summary>
/// 指定名の深度をクリア
/// </summary>
void DsvManager::Clear(
	const std::string& name,
	ID3D12GraphicsCommandList* commandList,
	D3D12_CLEAR_FLAGS flags)
{
	auto it = nameToIndex_.find(name);
	assert(it != nameToIndex_.end());

	Clear(it->second, commandList, flags);
}

/// <summary>
/// 深度リソースの状態遷移
/// </summary>
void DsvManager::TransitionResource(
	ID3D12GraphicsCommandList* commandList,
	uint32_t index,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after)
{
	assert(index < depthStencils_.size());

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		depthStencils_[index]->resource.Get(),
		before,
		after
	);

	commandList->ResourceBarrier(1, &barrier);
}

/// <summary>
/// 名前指定で深度バリア
/// </summary>
void DsvManager::TransitionBarrier(
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
/// インデックス指定で DSV 情報を取得
/// </summary>
const DsvManager::DepthStencil* DsvManager::Get(uint32_t index) const
{
	if (index >= depthStencils_.size()) return nullptr;
	return depthStencils_[index].get();
}

/// <summary>
/// 名前指定で DSV 情報を取得
/// </summary>
const DsvManager::DepthStencil* DsvManager::Get(const std::string& name) const
{
	auto it = nameToIndex_.find(name);
	if (it == nameToIndex_.end()) return nullptr;
	return depthStencils_[it->second].get();
}

/// <summary>
/// 名前 → インデックス取得
/// </summary>
uint32_t DsvManager::GetIndex(const std::string& name) const
{
	auto it = nameToIndex_.find(name);
	if (it == nameToIndex_.end()) return UINT32_MAX;
	return it->second;
}

/// <summary>
/// インデックス指定で CPU ハンドル取得
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE DsvManager::GetHandle(uint32_t index) const
{
	assert(index < depthStencils_.size());
	return depthStencils_[index]->dsvHandle;
}

/// <summary>
/// 名前指定で CPU ハンドル取得
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE DsvManager::GetHandle(const std::string& name) const
{
	auto it = nameToIndex_.find(name);
	assert(it != nameToIndex_.end());

	return GetHandle(it->second);
}

/// <summary>
/// DSV ディスクリプタヒープの作成
/// </summary>
void DsvManager::CreateHeap(uint32_t maxCount)
{
	auto device = deviceManager_->GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.NumDescriptors = maxCount;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap_));
	if (FAILED(hr)) {
		assert(SUCCEEDED(hr) && "Failed to create DSV Heap");
	}
	descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

/// <summary>
/// DepthStencil リソースの作成
/// </summary>
Microsoft::WRL::ComPtr<ID3D12Resource>
DsvManager::CreateDepthStencilTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format)
{
	D3D12_RESOURCE_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.DepthOrArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clear{};
	clear.DepthStencil.Depth = 1.0f;
	clear.Format = format;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = deviceManager_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clear,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		assert(SUCCEEDED(hr));
	}
	return resource;
}
