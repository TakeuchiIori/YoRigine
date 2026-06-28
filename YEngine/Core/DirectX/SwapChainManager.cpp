#include "SwapChainManager.h"

// Engine
#include "DeviceManager.h"
#include "CommandManager.h"
#include "WinApp/WinApp.h"

/// <summary>
/// スワップチェーンの初期化
/// </summary>
void SwapChainManager::Initialize(WinApp* winApp, DeviceManager* deviceManager, CommandManager* commandManager)
{
	this->winApp_ = winApp;
	this->deviceManager_ = deviceManager;
	this->commandManager_ = commandManager;

	// スワップチェーンを作成
	CreateSwapChain();
}

/// <summary>
/// スワップチェーン関連リソースの解放
/// </summary>
void SwapChainManager::Finalize()
{
	// スワップチェーンの解放
	swapChain_.Reset();

	// バックバッファの解放
	for (auto& resource : swapChainResources_) {
		resource.Reset();
	}
}

/// <summary>
/// スワップチェーンの生成処理
/// </summary>
void SwapChainManager::CreateSwapChain()
{
	HRESULT hr;

	// スワップチェーン設定
	swapChainDesc.Width = WinApp::kClientWidth;                     // 画面の幅（クライアント領域に合わせる）
	swapChainDesc.Height = WinApp::kClientHeight;                   // 画面の高さ（クライアント領域に合わせる）
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;              // バックバッファの色フォーマット
	swapChainDesc.SampleDesc.Count = 1;                             // マルチサンプルなし
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;    // 描画ターゲットとして使用
	swapChainDesc.BufferCount = backBuffers_;                       // バッファ数（ダブルバッファ）
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;       // 表示後バッファ内容を破棄する（推奨）

	// スワップチェーンの生成
	hr = deviceManager_->GetDXGIFactory()->CreateSwapChainForHwnd(
		commandManager_->GetCommandQueue().Get(),                       // コマンドキュー
		winApp_->GetHwnd(),                                             // 対象ウィンドウ
		&swapChainDesc,                                                 // スワップチェーン設定
		nullptr,                                                        // フルスクリーン用（未使用）
		nullptr,                                                        // 出力先アダプタ（未使用）
		reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	// バックバッファの取得
	for (int i = 0; i < backBuffers_; ++i) {
		hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		assert(SUCCEEDED(hr));
	}
}
