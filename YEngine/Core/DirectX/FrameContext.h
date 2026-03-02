#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <cassert>

// フレームコンテキスト
struct FrameComtext {

	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;

	// フェンス値
	uint64_t fenceValue = 0;

	// バックバッファのインデックス
	uint32_t backBufferIndex = 0;

	// フレームがGPU上で処理中かどうか
	bool isProcessing = false;

	// フレームコンテキスト初期化
	void Initialize(ID3D12Device* device, uint32_t index) {
		backBufferIndex = index;

		// コマンドアロケータ生成：結果を必ずチェックする
		HRESULT hr = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&commandAllocator)
		);
		if (FAILED(hr)) {
			assert(false && "Failed to CreateCommandAllocator");
		}
	}

	// GPU処理の完了待機
	void WaitForGPU(ID3D12Fence* fence, HANDLE fenceEvent) {
		if (fenceValue != 0 && fence->GetCompletedValue() < fenceValue)
		{
			fence->SetEventOnCompletion(fenceValue, fenceEvent);
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		isProcessing = false;
	}

	// フレーム開始時にコマンドアロケータをリセット
	void Reset() {
		if (!isProcessing) {
			HRESULT hr = commandAllocator->Reset();
			if (FAILED(hr)) {
				assert(SUCCEEDED(hr) && "Failed to Reset CommandAllocator");
			}
		}
	}
};