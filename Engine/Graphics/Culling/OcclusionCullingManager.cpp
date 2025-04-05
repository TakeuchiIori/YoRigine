#include "OcclusionCullingManager.h"
#include <DX/DirectXCommon.h>

void OcclusionCullingManager::Initialize()
{
	UINT queryCount = currentIndex_;
	queryCount_ = queryCount;
	//occlusionResults_.resize(queryCount_, 0);

	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice().Get();
	commandList_ = DirectXCommon::GetInstance()->GetCommandList().Get();

	// クエリの数をオブジェクト数に合わせる
	occlusionResults_.resize(queryCount, 0);
	wasVisibleLastFrame_.resize(queryCount, true);

	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Count = queryCount_;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	queryHeapDesc.NodeMask = 0;

	// クエリヒープ作成
	HRESULT hr = device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_));
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create Query Heap.");
	}

	// クエリ結果格納用のバッファを作成
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = sizeof(UINT64) * queryCount; // クエリ数に応じたサイズ
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_READBACK };

	hr = device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&queryResultBuffer_)
	);

	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create Query Result Buffer.");
	}
}

void OcclusionCullingManager::BeginOcclusionQuery(UINT queryIndex)
{
	 assert(queryIndex < queryCount_ && "BeginQuery: queryIndex out of range!");
	commandList_->BeginQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
	queriedFlags_[queryIndex] = true;
}

void OcclusionCullingManager::EndOcclusionQuery(UINT queryIndex)
{
	commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
}

void OcclusionCullingManager::ResolvedOcclusionQuery()
{
	// クエリ結果をリソースにコピー
	commandList_->ResolveQueryData
	(
		queryHeap_.Get(),
		D3D12_QUERY_TYPE_OCCLUSION,
		0,
		currentIndex_,
		queryResultBuffer_.Get(),
		0
	);

	// 結果をマッピング
	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, sizeof(UINT64) * queryCount_ };
	if (SUCCEEDED(queryResultBuffer_->Map(0, &readRange, &mappedData)))
	{
		UINT64* queryData = static_cast<UINT64*>(mappedData);
		for (uint32_t i = 0; i < queryCount_; i++) {
			bool wasVisible = (queryData[i] > 0);
			occlusionResults_[i] = queryData[i];

			// ★連続不可視の更新
			if (queriedFlags_[i]) {
				if (wasVisible) {
					invisibleCounts_[i] = 0;
				}else {
					invisibleCounts_[i]++;
				}
				wasVisibleLastFrame_[i] = wasVisible;
			}
		}
		queryResultBuffer_->Unmap(0, nullptr);
	}
}

void OcclusionCullingManager::RebuildResources(UINT newCount)
{
	queryCount_ = newCount;

	
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice().Get();
	
	occlusionResults_.resize(queryCount_);
	queriedFlags_.resize(queryCount_, false);
	invisibleCounts_.resize(queryCount_, 0);

	// クエリヒープ再作成
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Count = queryCount_;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	queryHeapDesc.NodeMask = 0;

	queryHeap_.Reset();
	HRESULT hr = device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_));
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to recreate Query Heap.");
	}

	// 結果バッファ再作成
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = sizeof(UINT64) * queryCount_;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_READBACK };
	queryResultBuffer_.Reset();
	hr = device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&queryResultBuffer_)
	);
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to recreate Query Result Buffer.");
	}

	occlusionResults_.resize(queryCount_, 0);
}
