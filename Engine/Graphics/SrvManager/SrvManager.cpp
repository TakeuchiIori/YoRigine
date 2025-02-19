#include "SrvManager.h"
#include "Loaders./Texture./TextureManager.h"

SrvManager* SrvManager::instance = nullptr;
const uint32_t SrvManager::kMaxSRVCount_ = 512;

SrvManager* SrvManager::GetInstance()
{
	if (instance == nullptr) {
		instance = new SrvManager;
	}
	return instance;

}

void SrvManager::Finalize()
{
	delete instance;
	instance = nullptr;
}


void SrvManager::Initialize(DirectXCommon* dxCommon)
{
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	// デスクリプタヒープの生成
	descriptorHeap_ = dxCommon_->CreateDescriptorHeap(dxCommon_->GetDevice(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount_,true);
	// デスクリプタ1個分のサイズを取得して記録
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

void SrvManager::PreDraw()
{

	// 描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUSRVDescriptorHandle(srvIndex));
}

uint32_t SrvManager::Allocate()
{
	// 上限に達していないかチェックしてassert
	assert(kMaxSRVCount_ > useIndex_);

	// return する番号を一旦記録しておく
	int index = useIndex_;
	// 次回のために番号を1進める
	useIndex_++;
	// 上で記録して番号をreturn
	return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUSRVDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE  handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUSRVDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE  handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);
	return handleGPU;
}

bool SrvManager::IsAllocation()
{
	if (kMaxSRVCount_ > useIndex_) {
		return true;
	}
	else {
		return false;
	}
	
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dtexture
	srvDesc.Texture2D.MipLevels = MipLevels;

	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUSRVDescriptorHandle(srvIndex));

}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファの場合、フォーマットは不明（UNKNOWN）
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; // バッファとしてビューを作成
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // デフォルトのシェーダマッピング
	srvDesc.Buffer.FirstElement = 0; // バッファ内の最初の要素のインデックス
	srvDesc.Buffer.NumElements = numElements; // バッファ内の要素数
	srvDesc.Buffer.StructureByteStride = structureByteStride; // 構造体のバイト単位のサイズ
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // 特殊なフラグは無し

	// SRV を作成
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUSRVDescriptorHandle(srvIndex));
}

