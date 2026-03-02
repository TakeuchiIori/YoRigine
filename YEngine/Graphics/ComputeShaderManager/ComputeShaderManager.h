#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <string>
#include <dxcapi.h>
#include <vector>
#include <unordered_map>
#include <array>

// Engine
#include "../Core/DirectX/DirectXCommon.h"

/// <summary>
/// CSの管理クラス
/// </summary>
class ComputeShaderManager
{
public:
	///************************* 基本関数 *************************///
	static ComputeShaderManager* GetInstance();
	void Initialize();
	ID3D12RootSignature* GetRootSignature(const std::string& key);
	ID3D12PipelineState* GetComputePipelineState(const std::string& key);
	void Finalize();

private:
	///************************* 内部処理 *************************///

	// スキニング
	void CreateSkinningCS();
	// パーティクル初期化
	void CreatePaticleInitCS();
	// パーティクル生成
	void CreateEmitCS();
	// パーティクル更新
	void CreateParticleUpdateCS();

private:
	// シングルトン
	ComputeShaderManager() = default;
	~ComputeShaderManager() = default;
	ComputeShaderManager(const ComputeShaderManager&) = delete;
	ComputeShaderManager& operator=(const ComputeShaderManager&) = delete;

private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> computePipelineStates_;
};


