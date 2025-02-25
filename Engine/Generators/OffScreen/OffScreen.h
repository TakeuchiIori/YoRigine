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

	void CreateBoxFilterResource();
	void CreateGaussFilterResource();
private:
	struct KernelForGPU {
		int kernelSize;
	};
	struct GaussKernelForGPU {
		int kernelSize;
		float sigma;
	};

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> boxResource_;
	KernelForGPU* boxData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> gaussResource_;
	GaussKernelForGPU* gaussData_ = nullptr;
};

