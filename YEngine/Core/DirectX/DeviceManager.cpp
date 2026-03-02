#include "DeviceManager.h"

// Engine
#include "Debugger/Logger.h"
#include "Debugger/ConvertString.h"

/// <summary>
/// DirectX12 デバイス・DXGI ファクトリー・デバッグレイヤーの初期化
/// </summary>
void DeviceManager::Initialize()
{
#ifdef _DEBUG
	//-------------------- デバッグレイヤー有効化 --------------------//

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();				// CPU 側のデバッグレイヤー
		debugController->SetEnableGPUBasedValidation(true); // GPU 側検証も有効化
	}
#endif

	//-------------------- DXGI ファクトリー生成 --------------------//

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr));

	//-------------------- 高性能 GPU アダプタ探索 --------------------//

	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

	for (UINT i = 0;
		dxgiFactory_->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&useAdapter)
		) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		DXGI_ADAPTER_DESC3 desc{};
		hr = useAdapter->GetDesc3(&desc);
		assert(SUCCEEDED(hr));

		// ハードウェア GPU のみ採用（ソフトウェアアダプタは除外）
		if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Logger(ConvertString(std::format(
				L"Use Adapter: {}\n", desc.Description
			)));
			break;
		}
		useAdapter = nullptr;
	}

	assert(useAdapter != nullptr);

	//-------------------- デバイス生成（最も高い FL から挑戦） --------------------//

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};
	const char* levelNames[] = { "12.2", "12.1", "12.0" };

	for (size_t i = 0; i < _countof(levels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), levels[i], IID_PPV_ARGS(&device_));
		if (SUCCEEDED(hr)) {
			Logger(std::format("FeatureLevel : {}\n", levelNames[i]));
			break;
		}
	}

	assert(device_ != nullptr);
	Logger("Complete create D3D12Device!!!!!!\n");

#ifdef _DEBUG
	//-------------------- InfoQueue（警告・エラーで停止） --------------------//

	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {

		// 重大エラーはブレイク
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// Windows11 の DXGI × DX12 バグ対策（誤警告を無視）
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};

		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		infoQueue->PushStorageFilter(&filter);
	}
#endif
}

/// <summary>
/// 各種デバイスリソースの解放
/// </summary>
void DeviceManager::Finalize()
{
	device_.Reset();
	dxgiFactory_.Reset();
}
