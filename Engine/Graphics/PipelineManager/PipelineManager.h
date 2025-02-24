#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <unordered_map>


class DirectXCommon;
class PipelineManager
{
public: 
	// ブレンドモード構造体
	enum BlendMode {
		// 通常のブレンド
		kBlendModeNormal,
		// 加算
		kBlendModeAdd,
		// 減算
		kBlendModeSubtract,
		// 乗算
		kBlendModeMultiply,
		// スクリーン
		kBlendModeScreen,

		// 利用してはいけない
		kCount0fBlendMode,
	};
	// コンストラクタとデストラクタ
	 PipelineManager() = default;
	~ PipelineManager() = default;

	static  PipelineManager* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();
	D3D12_BLEND_DESC GetBlendDesc(BlendMode _mode);
	ID3D12RootSignature* GetRootSignature(const std::string& key);
	ID3D12PipelineState* GetPipeLineStateObject(const std::string& key);

private:

	//==================================================//
	/*				各パイプラインの作成					*/
	//==================================================//


	/// <summary>
	/// スプライト用パイプライン
	/// </summary>
	void CreatePSO_Sprite();

	/// <summary>
	/// オブジェクト用のパイプライン
	/// </summary>
	void CreatePSO_Object();

	/// <summary>
	/// ライン用のパイプライン
	/// </summary>
	void CreatePSO_Animation();

	/// <summary>
	/// ライン用のパイプライン
	/// </summary>
	void CreatePSO_Line();

	/// <summary>
	/// パーティクル用のパイプライン
	/// </summary>
	void CreatePSO_Particle();

	/*=================================================================

							オフスクリーン関連

	=================================================================*/
	/// <summary>
	/// オフスクリーン用のパイプライン
	/// </summary>
	//void CreatePSO_OffScreen();

	void CreatePSO_BaseOffScreen(
		const std::wstring& pixelShaderPath = L"",
		const std::string& pipelineKey = ""
	);

private:
	PipelineManager(const  PipelineManager&) = delete;
	PipelineManager& operator=(const  PipelineManager&) = delete;
	PipelineManager(PipelineManager&&) = delete;
	PipelineManager& operator=(PipelineManager&&) = delete;
	DirectXCommon* dxCommon_ = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rootSignatures_;

};

