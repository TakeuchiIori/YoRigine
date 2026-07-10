#include "SrvManager.h"
#include "DirectXCommon.h"
#include "Loaders./Texture./TextureManager.h"

SrvManager* SrvManager::instance = nullptr;
const uint32_t SrvManager::kMaxSRVCount_ = 512;

/// <summary>
/// SrvManager のシングルトンインスタンス取得
/// </summary>
SrvManager* SrvManager::GetInstance()
{
	if (instance == nullptr) {
		instance = new SrvManager;
	}
	return instance;
}

/// <summary>
/// SrvManager の終了処理
/// </summary>
void SrvManager::Finalize()
{
	delete instance;
	instance = nullptr;
}

/// <summary>
/// SRV 用ディスクリプタヒープの初期化
/// </summary>
void SrvManager::Initialize(YoRigine::DirectXCommon* dxCommon)
{
	// すでに初期化済みなら何もしない
	if (dxCommon_ != nullptr) {
		return;
	}

	// DirectXCommon のポインタを保持
	dxCommon_ = dxCommon;

	// デスクリプタヒープの生成
	descriptorHeap_ = dxCommon_->GetDescriptorHeap()->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		kMaxSRVCount_,
		true
	);

	// デスクリプタ1個分のサイズを取得
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
}

/// <summary>
/// SRV ディスクリプタヒープをコマンドリストにセット
/// </summary>
void SrvManager::PreDraw()
{
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(
		_countof(descriptorHeaps),
		descriptorHeaps
	);
}

/// <summary>
/// 指定したルートパラメータに SRV をセット
/// </summary>
void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
		RootParameterIndex,
		GetGPUDescriptorHandle(srvIndex)
	);
}

/// <summary>
/// SRV を 1 個だけ確保
/// </summary>
uint32_t SrvManager::Allocate()
{
	return Allocate(1);
}

/// <summary>
/// SRV を複数個まとめて確保
/// </summary>
uint32_t SrvManager::Allocate(uint32_t count)
{
	assert(kMaxSRVCount_ >= useIndex_ + count);

	uint32_t index = useIndex_;
	useIndex_ += count;

	return index;
}

/// <summary>
/// 指定インデックスの SRV CPU ハンドル取得
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<SIZE_T>(descriptorSize_) * index;
	return handleCPU;
}

/// <summary>
/// 指定インデックスの SRV GPU ハンドル取得
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<UINT64>(descriptorSize_) * index;
	return handleGPU;
}

/// <summary>
/// まだ SRV を確保できるか判定
/// </summary>
bool SrvManager::IsAllocation()
{
	return (kMaxSRVCount_ > useIndex_);
}

/// <summary>
/// 2D テクスチャ用 SRV の作成
/// </summary>
void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DirectX::TexMetadata metadata)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2D Texture
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
	}

	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

/// <summary>
/// 構造化バッファ用 SRV の作成
/// </summary>
void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;								// 構造化バッファは UNKNOWN
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;					// バッファとして扱う
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;										// 先頭要素
	srvDesc.Buffer.NumElements = numElements;								// 要素数
	srvDesc.Buffer.StructureByteStride = structureByteStride;               // 1要素サイズ
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

/// <summary>
/// レンダーテクスチャ用 SRV の作成
/// </summary>
void SrvManager::CreateSRVforRenderTexture(uint32_t srvIndex, ID3D12Resource* pResource)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = pResource->GetDesc().Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

/// <summary>
/// 深度テクスチャ用 SRV の作成
/// </summary>
void SrvManager::CreateSRVforDepth(uint32_t srvIndex, ID3D12Resource* pResource)
{
	// リソースのフォーマット取得
	const auto resourceDesc = pResource->GetDesc();
	DXGI_FORMAT resourceFormat = resourceDesc.Format;

	DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

	// フォーマットに応じて選択
	switch (resourceFormat)
	{
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_TYPELESS:
		srvFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
		srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	default:
		assert(false && "この深度フォーマットはSRVに対応していません");
		break;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = srvFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}

/// <summary>
/// Texture2DArray 深度用 SRV の作成（シャドウカスケード）
/// </summary>
void SrvManager::CreateSRVforDepthArray(uint32_t srvIndex, ID3D12Resource* pResource, UINT arraySize)
{
	const auto resourceDesc = pResource->GetDesc();
	DXGI_FORMAT resourceFormat = resourceDesc.Format;

	DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
	switch (resourceFormat)
	{
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_TYPELESS:
		srvFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
		srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	default:
		assert(false && "この深度フォーマットはSRVに対応していません");
		break;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = srvFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = arraySize;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex)
	);
}


/// <summary>
/// 構造化バッファ用 UAV の作成
/// </summary>
void SrvManager::CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		pResource,
		nullptr,
		&uavDesc,
		GetCPUDescriptorHandle(uavIndex)
	);
}

/// <summary>
/// レンダーテクスチャ用 UAV の作成 (Texture2D / CS書き込み先)
/// SRGBフォーマットでは UAV を作れないため、format には UNORM 等の対応フォーマットを指定する。
/// 呼び出し側は TYPELESS リソースを用意し、RTV/SRV/UAV それぞれに適切な型付きビューを張る想定。
/// </summary>
void SrvManager::CreateUAVforRenderTexture(uint32_t uavIndex, ID3D12Resource* pResource, DXGI_FORMAT format)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		pResource,
		nullptr,
		&uavDesc,
		GetCPUDescriptorHandle(uavIndex)
	);
}
