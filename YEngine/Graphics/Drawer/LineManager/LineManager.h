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

namespace YoRigine {
	class DirectXCommon;
}
/// <summary>
/// ラインのパイプライン設定クラス
/// </summary>
class LineManager
{
public:
	///************************* 基本関数 *************************///
	static LineManager* GetInstance();
	LineManager() = default;
	~LineManager() = default;
	void Initialize();
public:
	///************************* アクセッサ *************************///
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() { return rootSignature_.Get(); }
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetGraphicsPiplineState() { return graphicsPipelineState_.Get(); }
private:
	// シングルトン
	LineManager(const LineManager&) = delete;
	LineManager& operator=(const LineManager&) = delete;
	LineManager(LineManager&&) = delete;
	LineManager& operator=(LineManager&&) = delete;
private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

};

