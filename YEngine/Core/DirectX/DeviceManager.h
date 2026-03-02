#pragma once

// Engine
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>
#include <string>
#include <cassert>
#include <thread>
#include <format>

/// <summary>
/// デバイス管理クラス
/// </summary>
class DeviceManager
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Finalize();
public:

	///************************* アクセッサ *************************///
	Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() { return device_; }
	Microsoft::WRL::ComPtr<IDXGIFactory7> GetDXGIFactory() { return dxgiFactory_; }

private:

	///************************* 基盤 *************************///

	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	// DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

};

