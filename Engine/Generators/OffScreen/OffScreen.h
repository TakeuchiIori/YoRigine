#pragma once
#include "wrl.h"
#include <d3d12.h>

class DirectXCommon;
class OffScreen
{
public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

private:

	void CreateSmoothResource();
private:
	struct KernelForGPU {
		int kernelSize;
	};

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> smoothResource_;
	KernelForGPU* smoothData_ = nullptr;
};

