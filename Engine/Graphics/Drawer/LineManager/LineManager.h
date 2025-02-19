#pragma once

// C++
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <vector>
#include <random>
#include <list>
#include <unordered_map >

class DirectXCommon;
class LineManager
{

public: // メンバ関数
	// コンストラクタとデストラクタ
	LineManager() = default;
	~LineManager() = default;

	static LineManager* GetInstance();
	LineManager(const LineManager&) = delete;
	LineManager& operator=(const LineManager&) = delete;
	LineManager(LineManager&&) = delete;
	LineManager& operator=(LineManager&&) = delete;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

public:

	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() { return rootSignature_.Get(); }
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetGraphicsPiplineState() { return graphicsPipelineState_.Get(); }


private: 
	// 外部からのポインタ
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

};

