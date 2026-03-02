#pragma once

// C++
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <dxgi1_6.h>

class WinApp;
class DeviceManager;
class CommandManager;

/// <summary>
/// スワップチェーン管理クラス
/// </summary>
class SwapChainManager
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize(WinApp* winApp, DeviceManager* deviceManager, CommandManager* commandManager);
	void Finalize();

private:
	///************************* 内部処理 *************************///

	// スワップチェーンの生成
	void CreateSwapChain();


public:
	///************************* アクセッサ *************************///
	Microsoft::WRL::ComPtr<IDXGISwapChain4> GetSwapChain() { return swapChain_; }
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>& GetSwapChainResources() { return swapChainResources_; }
	const std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>& GetSwapChainResources() const { return swapChainResources_; }
	int GetBackBuffers() const { return backBuffers_; }
	UINT GetBackBufferIndex() const { return backBufferIndex_; }
	UINT GetBackBufferCount()const { return  backBufferIndex_; }
	UINT GetCurrentBackBufferIndex() const { return swapChain_->GetCurrentBackBufferIndex(); }

private:
	///************************* メンバ変数 *************************///
	WinApp* winApp_ = nullptr;
	DeviceManager* deviceManager_ = nullptr;
	CommandManager* commandManager_ = nullptr;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	int backBuffers_ = 2;														// ダブルバッファ
	UINT backBufferIndex_ = backBuffers_;


};

