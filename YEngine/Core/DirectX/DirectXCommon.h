#pragma once
// C++
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <chrono>
#include <mutex>
#include <memory>

// Engine
#include "WinApp/WinApp.h"
#include "DeviceManager.h"
#include "CommandManager.h"
#include "SwapChainManager.h"
#include "SrvManager.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "DescriptorHeap.h"

// Math
#include "Vector4.h"

namespace YoRigine {

	/// <summary>
	/// DirectX12 基盤クラス（シングルトン）
	///
	/// ■ 責務
	///   ・デバイス / コマンド / スワップチェーン / 各種マネージャーの生存管理
	///   ・毎フレームの描画パス切り替え（PreDrawShadow / PreDrawOffScreen / PreDraw / PostDraw）
	///   ・RTV / DSV / SRV へのアクセス窓口の一本化
	///     → 外部コードは GetRTVManager() / GetDSVManager() を直接触らず、
	///       DirectXCommon のアクセッサを使うことを推奨
	///
	/// ■ 描画パスの流れ（MyGame::Draw より）
	///   PreDrawShadow()      シャドウパス（DSV のみ）
	///   PreDrawOffScreen()   オフスクリーン 3D パス
	///   PreDraw()            ポストエフェクト / UI パス（バックバッファへ）
	///   PostDraw()           Present → GPU Wait → コマンドリストリセット
	///
	/// ■ PickBuffer との連携
	///   PickBuffer は独自の RTV / DSV を持つため DirectXCommon には登録しない。
	///   ただし CommandList / CommandQueue / Device は DirectXCommon 経由で取得する。
	/// </summary>
	class DirectXCommon
	{
		// =========================================================================
		// 基本ライフサイクル
		// =========================================================================
	public:
		static DirectXCommon* GetInstance();

		DirectXCommon() = default;
		~DirectXCommon() = default;

		void Initialize(WinApp* winApp);
		void Finalize();

		// =========================================================================
		// 毎フレーム描画パス
		// =========================================================================
	public:
		/// シャドウマップパス開始（DSVのみセット・クリア・ビューポート設定）
		void PreDrawShadow();

		/// オフスクリーンパス開始（OffScreen RTV + MainDepth DSV）
		void PreDrawOffScreen();

		/// バックバッファパス開始（OffScreen / Depth を SRV 化 → BackBuffer RTV）
		void PreDraw();

		/// PiP (Picture-in-Picture) 2nd オフスクリーンパス開始。
		/// 指定 RT を RENDER_TARGET にバインドし、専用 DSV をクリアして
		/// 指定解像度のビューポート/シザーを設定する。
		void PreDrawPip(const std::string& rtName, const std::string& dsvName,
			uint32_t width, uint32_t height);

		/// PiP パス終了 (RT を GENERIC_READ に戻し ImGui::Image で表示可能にする)。
		void EndPipPass(const std::string& rtName);

		/// フレーム終了（Present → Signal → Wait → Reset）
		void PostDraw();

		// =========================================================================
		// 描画ユーティリティ
		// =========================================================================
	public:
		/// MainDepth を DEPTH_WRITE に戻してバックバッファと再バインド
		void DepthBarrier();

		/// ソフトパーティクル用：オフスクリーンパス中に MainDepth をシェーダ読み取り状態へ遷移し、
		/// OffScreen RT を「深度なし」で再バインドする。粒 PS が深度をサンプルして
		/// 遮蔽判定とソフトフェードを行うための準備。粒描画の直前に呼ぶ。
		void BeginParticleSoftDepth();

		/// ソフトパーティクル描画後の後始末：MainDepth を DEPTH_WRITE に戻し、
		/// OffScreen RT + MainDepth DSV を再バインドする。粒描画の直後に呼ぶ。
		void EndParticleSoftDepth();

		/// バックバッファの内容を FinalResult テクスチャにコピー
		void CopyBackBufferToFinalResult();

		// =========================================================================
		// リソースバリア
		// =========================================================================
	public:
		/// 汎用トランジションバリア（生ポインタ版）
		void TransitionBarrier(ID3D12Resource* pResource,
			D3D12_RESOURCE_STATES before,
			D3D12_RESOURCE_STATES after);

		/// UAV バリア
		void BarrierTypeUAV(ID3D12Resource* pResource);

		// =========================================================================
		// コマンドリスト制御（シーン遷移時などに使用）
		// =========================================================================
	public:
		/// コマンドリストを Close して Execute する
		void ExecuteCommandList();

		/// GPU の完了を待つ（現在フレームのフェンス待ち）
		void WaitForGPU();

		/// コマンドアロケータ＋コマンドリストをリセットする
		void ResetCommandList();

		/// 現在のコマンドリストを Execute → 新しい fence 値を signal → 完了待ち → Reset → SRV heap 再 bind。
		/// フレーム途中で本当に GPU 完了を待ちたい場合 (例: PiP の 2nd 描画前に CB stomp を回避) に使う。
		void FlushAndWait();

		// =========================================================================
		// リソース生成ヘルパー
		// =========================================================================
	public:
		/// シェーダーのコンパイル
		Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
			const std::wstring& filePath,
			const wchar_t* profile);

		/// Upload ヒープ上のバッファリソース生成
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

		/// Default ヒープ上の UAV バッファリソース生成
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResourceUAV(size_t sizeInBytes);

		// =========================================================================
		// デバイス / コマンド アクセッサ
		// =========================================================================
	public:
		Microsoft::WRL::ComPtr<ID3D12Device>               GetDevice() { return deviceManager_->GetDevice(); }
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>  GetCommandList() { return commandManager_->GetCommandList(); }
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>         GetCommandQueue() { return commandManager_->GetCommandQueue(); }
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     GetCommandAllocator() { return commandManager_->GetCurrentCommandAllocator(); }

		Microsoft::WRL::ComPtr<IDxcUtils>          GetDxcUtils() { return dxcUtils_; }
		Microsoft::WRL::ComPtr<IDxcCompiler3>      GetDxcCompiler() { return dxcCompiler_; }
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> GetIncludeHandler() { return includeHandler_; }

		// =========================================================================
		// スワップチェーン アクセッサ
		// =========================================================================
	public:
		Microsoft::WRL::ComPtr<IDXGISwapChain4>                         GetSwapChain() { return swapChainManager_->GetSwapChain(); }
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2>           GetSwapChainResources() { return swapChainManager_->GetSwapChainResources(); }
		UINT GetBackBufferCount()          const { return swapChainManager_->GetBackBufferCount(); }
		UINT GetCurrentBackBufferIndex()   const { return swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex(); }

		// =========================================================================
		// マネージャー アクセッサ
		//   ※ 外部から直接触る場合は名前付きアクセッサを優先してください
		// =========================================================================
	public:
		DescriptorHeap* GetDescriptorHeap() { return descriptorHeap_.get(); }
		DeviceManager* GetDeviceManager() { return deviceManager_.get(); }
		SrvManager* GetSrvManager() { return srvManager_; }

		/// RTV マネージャーの直接取得（やむを得ない場合のみ使用）
		RtvManager* GetRTVManager() { return rtvManager_.get(); }
		/// DSV マネージャーの直接取得（やむを得ない場合のみ使用）
		DsvManager* GetDSVManager() { return dsvManager_.get(); }

		// =========================================================================
		// 名前付き RTV アクセッサ
		//   RtvManager を外部に晒さず、名前で直接取得できる窓口
		// =========================================================================
	public:
		/// 名前指定で RTV リソースを取得
		ID3D12Resource* GetRTVResource(const std::string& name);
		/// 名前指定で RTV ハンドルを取得（OMSetRenderTargets 用）
		D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(const std::string& name);
		/// 名前指定で RTV の SRV（GPU ハンドル）を取得（シェーダーバインド用）
		D3D12_GPU_DESCRIPTOR_HANDLE GetRTVSrvGPU(const std::string& name);
		/// 名前指定で RTV の SRV（CPU ハンドル）を取得
		D3D12_CPU_DESCRIPTOR_HANDLE GetRTVSrvCPU(const std::string& name);

		// =========================================================================
		// 名前付き DSV アクセッサ
		// =========================================================================
	public:
		/// 名前指定で DSV リソースを取得
		ID3D12Resource* GetDSVResource(const std::string& name);
		/// 名前指定で DSV ハンドルを取得（OMSetRenderTargets 用）
		D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(const std::string& name);
		/// 名前指定で DSV の SRV（GPU ハンドル）を取得（シェーダーバインド用）
		D3D12_GPU_DESCRIPTOR_HANDLE GetDSVSrvGPU(const std::string& name);
		/// 名前指定で DSV の SRV（CPU ハンドル）を取得
		D3D12_CPU_DESCRIPTOR_HANDLE GetDSVSrvCPU(const std::string& name);

		// =========================================================================
		// よく使うバッファの便利アクセッサ（後方互換 + 可読性向上）
		//   内部では GetRTVSrv / GetDSVSrv を呼んでいるだけ
		// =========================================================================
	public:
		D3D12_GPU_DESCRIPTOR_HANDLE GetOffScreenGPUHandle() { return GetRTVSrvGPU("OffScreen"); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetOffScreenCPUHandle() { return GetRTVSrvCPU("OffScreen"); }
		D3D12_GPU_DESCRIPTOR_HANDLE GetNormalGPUHandle() { return GetRTVSrvGPU("NormalBuffer"); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetNormalCPUHandle() { return GetRTVSrvCPU("NormalBuffer"); }
		D3D12_GPU_DESCRIPTOR_HANDLE GetDepthGPUHandle() { return GetDSVSrvGPU("MainDepth"); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDepthCPUHandle() { return GetDSVSrvCPU("MainDepth"); }
		D3D12_GPU_DESCRIPTOR_HANDLE GetShadowDepthGPUHandle() { return GetDSVSrvGPU("ShadowDepth"); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetShadowDepthCPUHandle() { return GetDSVSrvCPU("ShadowDepth"); }
		D3D12_GPU_DESCRIPTOR_HANDLE GetFinalResultGPUHandle() { return GetRTVSrvGPU("FinalResult"); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetFinalResultCPUHandle() { return GetRTVSrvCPU("FinalResult"); }

		// =========================================================================
		// 内部処理
		// =========================================================================
	private:
		void InitializeManagers();
		void InitializeRenderTargets();
		void InitializeViewPortRectangle();
		void InitializeScissorRectangle();
		void CreateDXCompiler();
		void InitializeFixFPS();
		void UpdateFixFPS();

		// =========================================================================
		// シングルトン制御
		// =========================================================================
	private:
		static std::unique_ptr<DirectXCommon> instance;
		static std::once_flag                 initInstanceFlag;

		DirectXCommon(const DirectXCommon&) = delete;
		DirectXCommon& operator=(const DirectXCommon&) = delete;
		DirectXCommon(DirectXCommon&&) = delete;
		DirectXCommon& operator=(DirectXCommon&&) = delete;

		// =========================================================================
		// メンバ変数
		// =========================================================================
	private:
		WinApp* winApp_ = nullptr;

		// FPS 固定
		std::chrono::steady_clock::time_point reference_;
		const std::chrono::microseconds kMinTime{ static_cast<uint64_t>(1000000.0f / 60.0f) };
		const std::chrono::microseconds kMinCheckTime{ static_cast<uint64_t>(1000000.0f / 61.0f) };

		// マネージャー群
		std::unique_ptr<DeviceManager>    deviceManager_;
		std::unique_ptr<CommandManager>   commandManager_;
		std::unique_ptr<SwapChainManager> swapChainManager_;
		std::unique_ptr<DescriptorHeap>   descriptorHeap_;
		SrvManager* srvManager_ = nullptr;
		std::unique_ptr<RtvManager>       rtvManager_;
		std::unique_ptr<DsvManager>       dsvManager_;

		// ビューポート / シザー矩形
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT     scissorRect_{};
		D3D12_VIEWPORT shadowVP_{};
		D3D12_RECT     shadowSC_{};

		// バリア一時変数（毎フレーム使い回す）
		D3D12_RESOURCE_BARRIER barrier_{};

		// シェーダーコンパイラ
		Microsoft::WRL::ComPtr<IDxcUtils>          dxcUtils_;
		Microsoft::WRL::ComPtr<IDxcCompiler3>      dxcCompiler_;
		IDxcIncludeHandler* includeHandler_ = nullptr;

		// リソース状態のトラッキング
		// （TransitionBarrier で "before" を間違えないための保険）
		D3D12_RESOURCE_STATES depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		D3D12_RESOURCE_STATES shadowDepthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		D3D12_RESOURCE_STATES finalResultCurrentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
	};

} // namespace YoRigine