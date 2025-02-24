#pragma once
// C++
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <chrono>

// Engine
#include "WinApp./WinApp.h"
#include "SrvManager/SrvManager.h"

// DirectX
#include "DirectXTex.h"

// Math
#include "Vector4.h"

/// <summary>
/// DirectX基盤
/// </summary>
using namespace std;
class DirectXCommon
{
public: // 各種初期化

	/// <summary>
	/// インスタンスの取得（ユニークポインタを使ったシングルトン）
	/// </summary>
	static DirectXCommon* GetInstance();

	// シングルトンのコピー・ムーブ操作を削除
	DirectXCommon(const DirectXCommon&) = delete;
	DirectXCommon& operator=(const DirectXCommon&) = delete;
	DirectXCommon(DirectXCommon&&) = delete;
	DirectXCommon& operator=(DirectXCommon&&) = delete;

	/// <summary>
	/// DirectXの初期化
	/// </summary>
	void Initialize(WinApp* winApp);

private:
	/// <summary>
	/// DXGIデバイス初期化
	/// </summary>
	void InitializeDXGIDevice();//bool enableDebugLayer = true

	/// <summary>
	/// コマンド関連の初期化
	/// </summary>
	void InitializeCommand();

	/// <summary>
	/// スワップチェーンの生成
	/// </summary>
	void CreateSwapChain();

	/// <summary>
	/// レンダーターゲットの初期化
	/// </summary>
	void InitializeRenderTarget();

	/// <summary>
	/// 深度ステンシルビューの初期化
	/// </summary>
	void InitializeDepthStencilView();

	/// <summary>
	/// フェンスの初期化
	/// </summary>
	void InitializeFence();

	/// <summary>
	/// ビューポート矩形の初期化
	/// </summary>
	void InitializeViewPortRevtangle();

	/// <summary>
	/// シザリング矩形の初期化
	/// </summary>
	void InitializeScissorRevtangle();

	/// <summary>
	/// 深度バッファ生成
	/// </summary>
	void CreateDepthBuffer();

	/// <summary>
	/// 各種ディスクリプターヒープの生成
	/// </summary>
	void CreateDescriptorHeap();

	/// <summary>
	/// DXCコンパイラの生成
	/// </summary>
	void CreateDXCompiler();

	/// <summary>
	/// バリア
	/// </summary>
	/// <param name="resource"></param>
	/// <param name="beforeState"></param>
	/// <param name="afterState"></param>
	/// <param name="subresource"></param>
	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState);

	void BeginRenderTargetRTV(const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = nullptr);
	void EndRenderTargetRTV();

	// リソース状態管理
	
	void TransitionOffScreen(D3D12_RESOURCE_STATES newState);

public:
	
	/// <summary>
	/// 描画前処理
	/// </summary>
	void PreDrawScene();
	void PreDrawImGui();
	
	/// <summary>
	/// 描画後処理
	/// </summary>
	void PostDraw();


public: // メンバ関数
	/// <summary>
	/// ディスクリプターヒープ
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// 深度バッファのリソース
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, int32_t width, int32_t height);

	/// <summary>
	/// レンダーテクスチャ作成
	/// </summary>
	/// <returns></returns>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, D3D12_CLEAR_VALUE clearValue);

	/// <summary>
	/// 指定番号のCPUの取得
	/// </summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// 指定番号のGPUの取得
	/// </summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// DSVの指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVCPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// DSVの指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetDSVGPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// オフスクリーンのGPUハンドルを取得
	/// </summary>
	/// <returns></returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetOffScreenGPUHandle() { return offScreenSrvHandleGPU_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetOffScreenCPUHandle() { return offScreenSrvHandleCPU_; }

private:
	/// <summary>
	/// FPS固定初期化
	/// </summary>
	void InitializeFixFPS();

	/// <summary>
	/// FPS固定更新
	/// </summary>
	void UpdateFixFPS();

	// 記録時間（FPS固定用）
	std::chrono::steady_clock::time_point reference_;

public:
	/// <summary>
	/// シェーダーのコンパイル
	/// </summary>
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const wstring& filePath, const wchar_t* profile);

	/// <summary>
	/// バッファリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t  sizeInBytes);

	/// <summary>
	/// テクスチャリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	/// <summary>
	/// テクスチャデータの転送
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

	/// <summary>
	/// オフスクリーンのSRVを生成
	/// </summary>
	/// <param name="srvManager"></param>
	void CreateSRVForOffScreen();

	/// <summary>
	/// ログ
	/// </summary>
	/// <param name="message"></param>
	static void Log(const std::string& message);
	static std::wstring ConvertString(const std::string& str);
	static std::string ConvertString(const std::wstring& str);

public: // アクセッサ

	Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() { return device_; }
	Microsoft::WRL::ComPtr<IDxcUtils> GetDxcUtils() { return dxcUtils_; }
	Microsoft::WRL::ComPtr<IDxcCompiler3> GetDxcCompiler() { return dxcCompiler_; }
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> GetIncludeHandler() { return includeHandler_; }
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() { return commandList_; }
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return commandQueue_; }
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetCommandAllocator() { return commandAllocator_; }
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetrtvDescriptorHeap() const { return rtvDescriptorHeap_; }
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetdsvDescriptorHeap() const { return dsvDescriptorHeap_; }
	Microsoft::WRL::ComPtr<IDXGISwapChain4> GetSwapChain() { return swapChain_; }
	Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return fence_; }

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> GetSwapChainResources() { return swapChainResources_; }

	uint32_t GetdescriptotSizeRTV() const { return descriptotSizeRTV_; }
	uint32_t GetdescriptotSizeDSV() const { return descriptotSizeDSV_; }
	uint64_t GetFenceValue() { return fenceValue_; }
	uint64_t SetFenceValue(uint64_t val) { return fenceValue_ = val; }

	D3D12_CPU_DESCRIPTOR_HANDLE* GetrtvHandles() { return rtvHandles_; }
	HANDLE GetFenceEvent() { return fenceEvent_; }
	UINT GetBackBufferCount()const { return  backBufferIndex; }

private:
	/// <summary>
	/// デフォルトコンストラクタ（シングルトンパターンのためプライベートに設定）
	/// </summary>
	DirectXCommon() = default;

	/// <summary>
	/// デストラクタ（リソースの自動解放のために使用）
	/// </summary>
	~DirectXCommon() = default;


	/*=================================================================
	
								DirectX基盤

   =================================================================*/
   // WindowsAPI
	WinApp* winApp_ = nullptr;
	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	// DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

	/*=================================================================
	
								コマンド関連

	=================================================================*/
	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_;

	/*=================================================================
	
								スワップチェーン

	=================================================================*/
	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
	int backBuffers = 2; // ダブルバッファ
	UINT backBufferIndex = backBuffers;

	/*=================================================================
	
							 ディスクリプタヒープ

	=================================================================*/
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	uint32_t descriptotSizeRTV_;
	uint32_t descriptotSizeDSV_;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[3];
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};

	/*=================================================================
	
								深度バッファ

	=================================================================*/
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	/*=================================================================
	
								描画設定

	=================================================================*/
	// ビューポート
	D3D12_VIEWPORT viewport_{};
	// シザー矩形
	D3D12_RECT scissorRect_{};
	// TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier_{};

	/*=================================================================
	
								 シェーダー関連

	=================================================================*/
	// DXCコンパイラ関連
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	// 現時点ではincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler_ = nullptr;

	/*=================================================================

								オフスクリーン

	=================================================================*/
	D3D12_CLEAR_VALUE renderTargetClearColor_;
	Microsoft::WRL::ComPtr<ID3D12Resource> offScreenResource_;

	uint32_t offScreenSrvIndex_ = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE offScreenSrvHandleCPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE offScreenSrvHandleGPU_;
	// リソース状態の追跡用
	std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> resourceStates_;
	D3D12_RESOURCE_STATES offScreenState_ = D3D12_RESOURCE_STATE_COMMON;

};

