#include "DirectXCommon.h"

// C++
#include <cassert>
#include <thread>
#include <format>
#include "d3dx12.h"
#include "Debugger/Logger.h"
#include "Debugger/ConvertString.h"
#include <Debugger/DebugConsole.h>

// lib
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"dxcompiler.lib")

namespace YoRigine {

	std::unique_ptr<DirectXCommon> DirectXCommon::instance = nullptr;
	std::once_flag                 DirectXCommon::initInstanceFlag;

	// =========================================================================
	// シングルトン
	// =========================================================================
	DirectXCommon* DirectXCommon::GetInstance()
	{
		std::call_once(initInstanceFlag, []() {
			instance = std::make_unique<DirectXCommon>();
			});
		return instance.get();
	}

	// =========================================================================
	// Initialize / Finalize
	// =========================================================================
	void DirectXCommon::Initialize(WinApp* winApp)
	{
		assert(winApp);
		winApp_ = winApp;

		InitializeFixFPS();
		InitializeManagers();
		InitializeRenderTargets();
		InitializeViewPortRectangle();
		InitializeScissorRectangle();
		CreateDXCompiler();
	}

	void DirectXCommon::Finalize()
	{
		// コマンドマネージャーを先に終了してGPU完了を待つ
		if (commandManager_) {
			commandManager_->Finalize();
			commandManager_.reset();
		}

		// 各マネージャーを逆順で破棄
		if (dsvManager_) { dsvManager_->Finalize();      dsvManager_.reset(); }
		if (rtvManager_) { rtvManager_->Finalize();      rtvManager_.reset(); }
		if (srvManager_) { srvManager_->Finalize();      srvManager_ = nullptr; }
		if (swapChainManager_) { swapChainManager_->Finalize(); swapChainManager_.reset(); }
		if (deviceManager_) { deviceManager_->Finalize();   deviceManager_.reset(); }

		instance.reset();
	}

	// =========================================================================
	// InitializeManagers
	// =========================================================================
	void DirectXCommon::InitializeManagers()
	{
		deviceManager_ = std::make_unique<DeviceManager>();
		deviceManager_->Initialize();

		descriptorHeap_ = std::make_unique<DescriptorHeap>();
		descriptorHeap_->Initialize(this);

		commandManager_ = std::make_unique<CommandManager>();
		commandManager_->Initialize(deviceManager_.get());

		swapChainManager_ = std::make_unique<SwapChainManager>();
		swapChainManager_->Initialize(winApp_, deviceManager_.get(), commandManager_.get());

		srvManager_ = SrvManager::GetInstance();
		srvManager_->Initialize(this);

		rtvManager_ = std::make_unique<RtvManager>();
		rtvManager_->Initialize(deviceManager_.get(), 16);

		dsvManager_ = std::make_unique<DsvManager>();
		dsvManager_->Initialize(deviceManager_.get(), 8);
	}

	// =========================================================================
	// InitializeRenderTargets
	// =========================================================================
	void DirectXCommon::InitializeRenderTargets()
	{
		// バックバッファ（スワップチェーンリソースを登録）
		auto& resources = swapChainManager_->GetSwapChainResources();
		for (int i = 0; i < swapChainManager_->GetBackBuffers(); i++) {
			rtvManager_->Register(
				"BackBuffer" + std::to_string(i),
				resources[i],
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		}

		// オフスクリーン RTV
		rtvManager_->Create(
			"OffScreen",
			WinApp::kClientWidth, WinApp::kClientHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 0.1f, 0.1f, 0.2f, 1.0f },
			true);   // SRV も同時に作成

		// 最終出力 RTV（FinalResult）
		rtvManager_->Create(
			"FinalResult",
			WinApp::kClientWidth, WinApp::kClientHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 0.0f, 0.0f, 0.0f, 1.0f },
			true);

		// メイン深度バッファ
		dsvManager_->Create(
			"MainDepth",
			WinApp::kClientWidth, WinApp::kClientHeight,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			true);

		// シャドウマップ用深度バッファ
		dsvManager_->Create(
			"ShadowDepth",
			DsvManager::kShadowmapWidth, DsvManager::kShadowmapHeight,
			DXGI_FORMAT_D32_FLOAT,
			true);
	}

	// =========================================================================
	// 描画パス
	// =========================================================================

	/// シャドウパス：ShadowDepth DSV のみをセットしてクリア
	void DirectXCommon::PreDrawShadow()
	{
		auto cmd = commandManager_->GetCommandList();

		if (shadowDepthCurrentState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
			dsvManager_->TransitionBarrier(
				cmd.Get(), "ShadowDepth",
				shadowDepthCurrentState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			shadowDepthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvManager_->GetHandle("ShadowDepth");
		cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsvH);
		dsvManager_->Clear("ShadowDepth", cmd.Get());
		cmd->RSSetViewports(1, &shadowVP_);
		cmd->RSSetScissorRects(1, &shadowSC_);
	}

	/// オフスクリーンパス：OffScreen RTV + MainDepth DSV
	void DirectXCommon::PreDrawOffScreen()
	{
		auto cmd = commandManager_->GetCommandList();

#ifdef USE_IMGUI
		DebugConsole::GetInstance()->BeginFrame();
#endif

		// OffScreen を RenderTarget へ
		rtvManager_->TransitionBarrier(
			cmd.Get(), "OffScreen",
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		// MainDepth を DEPTH_WRITE へ
		if (depthCurrentState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
			dsvManager_->TransitionBarrier(
				cmd.Get(), "MainDepth",
				depthCurrentState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		// ShadowDepth をシェーダーから読み取れるように
		if (shadowDepthCurrentState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
			dsvManager_->TransitionBarrier(
				cmd.Get(), "ShadowDepth",
				shadowDepthCurrentState_,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			shadowDepthCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvManager_->GetHandle("MainDepth");
		rtvManager_->SetRenderTargets(cmd.Get(), { "OffScreen" }, &dsvH);
		rtvManager_->Clear("OffScreen", cmd.Get());
		dsvManager_->Clear("MainDepth", cmd.Get());

		cmd->RSSetViewports(1, &viewport_);
		cmd->RSSetScissorRects(1, &scissorRect_);
	}

	/// バックバッファパス：OffScreen / Depth を SRV 化 → BackBuffer RTV
	void DirectXCommon::PreDraw()
	{
		auto cmd = commandManager_->GetCommandList();
		UINT  idx = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string bb = "BackBuffer" + std::to_string(idx);

		// OffScreen → SRV
		rtvManager_->TransitionBarrier(
			cmd.Get(), "OffScreen",
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_GENERIC_READ);

		// MainDepth → SRV
		dsvManager_->TransitionBarrier(
			cmd.Get(), "MainDepth",
			depthCurrentState_,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		depthCurrentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		// BackBuffer → RTV
		rtvManager_->TransitionBarrier(
			cmd.Get(), bb,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvManager_->GetHandle("MainDepth");
		rtvManager_->SetRenderTargets(cmd.Get(), { bb }, &dsvH);

		auto* rt = rtvManager_->Get(bb);
		if (rt) {
			float white[] = { 1.f, 1.f, 1.f, 1.f };
			cmd->ClearRenderTargetView(rt->rtvHandle, white, 0, nullptr);
		}

		cmd->RSSetViewports(1, &viewport_);
		cmd->RSSetScissorRects(1, &scissorRect_);
	}

	/// フレーム終了
	void DirectXCommon::PostDraw()
	{
		if (!swapChainManager_ || !commandManager_) return;

		auto cmd = commandManager_->GetCommandList();
		auto queue = commandManager_->GetCommandQueue();
		UINT  idx = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string bb = "BackBuffer" + std::to_string(idx);

		// BackBuffer → Present
		rtvManager_->TransitionBarrier(
			cmd.Get(), bb,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);

		// コマンドリスト確定・実行
		HRESULT hr = cmd->Close();

		assert(SUCCEEDED(hr));
		if (FAILED(hr)) {
			// ログ出力やエラーハンドリング
			return;
		}
		ID3D12CommandList* lists[] = { cmd.Get() };
		queue->ExecuteCommandLists(1, lists);

		swapChainManager_->GetSwapChain()->Present(1, 0);

#ifdef USE_IMGUI
		DebugConsole::GetInstance()->EndFrame();
#endif

		commandManager_->EndFrame();
		commandManager_->WaitForAllFrames();
		UpdateFixFPS();
		commandManager_->Reset(idx);
	}

	// =========================================================================
	// 描画ユーティリティ
	// =========================================================================

	/// MainDepth を DEPTH_WRITE に戻してバックバッファと再バインド
	void DirectXCommon::DepthBarrier()
	{
		auto cmd = commandManager_->GetCommandList();
		UINT  idx = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string bb = "BackBuffer" + std::to_string(idx);

		dsvManager_->TransitionBarrier(
			cmd.Get(), "MainDepth",
			depthCurrentState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthCurrentState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvManager_->GetHandle("MainDepth");
		rtvManager_->SetRenderTargets(cmd.Get(), { bb }, &dsvH);
		cmd->RSSetViewports(1, &viewport_);
		cmd->RSSetScissorRects(1, &scissorRect_);
	}

	/// バックバッファを FinalResult テクスチャへコピー
	void DirectXCommon::CopyBackBufferToFinalResult()
	{
		auto cmd = commandManager_->GetCommandList();
		UINT  idx = swapChainManager_->GetSwapChain()->GetCurrentBackBufferIndex();
		std::string bb = "BackBuffer" + std::to_string(idx);

		// BackBuffer → COPY_SOURCE
		rtvManager_->TransitionBarrier(
			cmd.Get(), bb,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE);

		// FinalResult → COPY_DEST
		rtvManager_->TransitionBarrier(
			cmd.Get(), "FinalResult",
			finalResultCurrentState_,
			D3D12_RESOURCE_STATE_COPY_DEST);

		auto* bbRT = rtvManager_->Get(bb);
		auto* frRT = rtvManager_->Get("FinalResult");
		if (bbRT && frRT) {
			cmd->CopyResource(frRT->resource.Get(), bbRT->resource.Get());
		}

		// 元の状態に戻す
		rtvManager_->TransitionBarrier(
			cmd.Get(), bb,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		rtvManager_->TransitionBarrier(
			cmd.Get(), "FinalResult",
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_GENERIC_READ);
		finalResultCurrentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	// =========================================================================
	// リソースバリア
	// =========================================================================
	void DirectXCommon::TransitionBarrier(
		ID3D12Resource* pResource,
		D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after)
	{
		barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier_.Transition.pResource = pResource;
		barrier_.Transition.StateBefore = before;
		barrier_.Transition.StateAfter = after;
		commandManager_->GetCommandList()->ResourceBarrier(1, &barrier_);
	}

	void DirectXCommon::BarrierTypeUAV(ID3D12Resource* pResource)
	{
		barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier_.Transition.pResource = pResource;
		commandManager_->GetCommandList()->ResourceBarrier(1, &barrier_);
	}

	// =========================================================================
	// コマンドリスト制御
	// =========================================================================
	void DirectXCommon::ExecuteCommandList()
	{
		HRESULT hr = commandManager_->GetCommandList()->Close();
		assert(SUCCEEDED(hr));
		if (FAILED(hr)) {
			// ログ出力やエラーハンドリング
			return;
		}
		ID3D12CommandList* lists[] = { commandManager_->GetCommandList().Get() };
		commandManager_->GetCommandQueue()->ExecuteCommandLists(1, lists);
	}

	void DirectXCommon::WaitForGPU()
	{
		commandManager_->WaitForCurrentFrame();
	}

	void DirectXCommon::ResetCommandList()
	{
		commandManager_->Reset(commandManager_->GetCurrentFrameIndex());
	}

	// =========================================================================
	// 名前付き RTV アクセッサ実装
	// =========================================================================
	ID3D12Resource* DirectXCommon::GetRTVResource(const std::string& name)
	{
		auto* rt = rtvManager_->Get(name);
		return rt ? rt->resource.Get() : nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVHandle(const std::string& name)
	{
		return rtvManager_->GetHandle(name);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVSrvGPU(const std::string& name)
	{
		auto* rt = rtvManager_->Get(name);
		return rt ? rt->srvHandleGPU : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetRTVSrvCPU(const std::string& name)
	{
		auto* rt = rtvManager_->Get(name);
		return rt ? rt->srvHandleCPU : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}

	// =========================================================================
	// 名前付き DSV アクセッサ実装
	// =========================================================================
	ID3D12Resource* DirectXCommon::GetDSVResource(const std::string& name)
	{
		auto* ds = dsvManager_->Get(name);
		return ds ? ds->resource.Get() : nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVHandle(const std::string& name)
	{
		return dsvManager_->GetHandle(name);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVSrvGPU(const std::string& name)
	{
		auto* ds = dsvManager_->Get(name);
		return ds ? ds->srvHandleGPU : D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDSVSrvCPU(const std::string& name)
	{
		auto* ds = dsvManager_->Get(name);
		return ds ? ds->srvHandleCPU : D3D12_CPU_DESCRIPTOR_HANDLE{};
	}

	// =========================================================================
	// リソース生成ヘルパー
	// =========================================================================
	Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile)
	{
		Logger(ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));

		IDxcBlobEncoding* shaderSource = nullptr;
		HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
		assert(SUCCEEDED(hr));

		DxcBuffer buf{};
		buf.Ptr = shaderSource->GetBufferPointer();
		buf.Size = shaderSource->GetBufferSize();
		buf.Encoding = DXC_CP_UTF8;

		LPCWSTR args[] = {
			filePath.c_str(),
			L"-E", L"main",
			L"-T", profile,
			L"-Zi", L"-Qembed_debug",
			L"-Od", L"-Zpr",
		};

		IDxcResult* result = nullptr;
		hr = dxcCompiler_->Compile(&buf, args, _countof(args), includeHandler_, IID_PPV_ARGS(&result));
		assert(SUCCEEDED(hr));

		Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		if (errors && errors->GetStringLength()) {
			Logger(errors->GetStringPointer());
			assert(false);
		}

		IDxcBlob* blob = nullptr;
		hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
		assert(SUCCEEDED(hr));

		Logger(ConvertString(std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile)));

		shaderSource->Release();
		result->Release();
		return blob;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
	{
		D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_UPLOAD };
		D3D12_RESOURCE_DESC   rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = sizeInBytes;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.SampleDesc.Count = 1;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		HRESULT hr = deviceManager_->GetDevice()->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));
		if (FAILED(hr)) {
			// ログ出力やエラーハンドリング
			return nullptr;
		}
		return resource;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResourceUAV(size_t sizeInBytes)
	{
		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC rd{};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = sizeInBytes;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.SampleDesc.Count = 1;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		GetDevice()->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));
		return resource;
	}

	// =========================================================================
	// ビューポート / シザー矩形 初期化
	// =========================================================================
	void DirectXCommon::InitializeViewPortRectangle()
	{
		viewport_ = { 0, 0,
			static_cast<float>(WinApp::kClientWidth),
			static_cast<float>(WinApp::kClientHeight),
			0.f, 1.f };

		shadowVP_ = { 0, 0,
			static_cast<float>(DsvManager::kShadowmapWidth),
			static_cast<float>(DsvManager::kShadowmapHeight),
			0.f, 1.f };
	}

	void DirectXCommon::InitializeScissorRectangle()
	{
		scissorRect_ = { 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
		shadowSC_ = { 0, 0, DsvManager::kShadowmapWidth, DsvManager::kShadowmapHeight };
	}

	// =========================================================================
	// DXC コンパイラ初期化
	// =========================================================================
	void DirectXCommon::CreateDXCompiler()
	{
		HRESULT hr;
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));    assert(SUCCEEDED(hr));
		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_)); assert(SUCCEEDED(hr));
		hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);            assert(SUCCEEDED(hr));
	}

	// =========================================================================
	// FPS 固定
	// =========================================================================
	void DirectXCommon::InitializeFixFPS()
	{
		reference_ = std::chrono::steady_clock::now();
	}

	void DirectXCommon::UpdateFixFPS()
	{
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

		if (elapsed < kMinCheckTime) {
			auto sleepTime = kMinCheckTime - elapsed;
			auto sleepEnd = now + sleepTime;
			if (sleepTime > std::chrono::microseconds(1000))
				std::this_thread::sleep_for(sleepTime - std::chrono::microseconds(1000));
			while (std::chrono::steady_clock::now() < sleepEnd) { /* spin */ }
		}

		reference_ += kMinCheckTime;
		if (std::chrono::steady_clock::now() > reference_ + kMinCheckTime)
			reference_ = std::chrono::steady_clock::now();
	}

} // namespace YoRigine