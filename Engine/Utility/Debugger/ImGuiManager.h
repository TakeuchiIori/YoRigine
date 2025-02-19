#pragma once

// C++
#ifdef _DEBUG
#include "imgui.h"
#include <wrl.h>
#include <d3d12.h>
#endif

class WinApp;
class DirectXCommon;
class ImGuiManager
{
public: // メンバ関数

	static ImGuiManager* GetInstance();
	ImGuiManager() = default;
	~ImGuiManager() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(WinApp* winApp,DirectXCommon* dxCommon);

	/// <summary>
	/// ImGui受付開始
	/// </summary>
	void Begin();

	/// <summary>
	/// ImGui受付終了
	/// </summary>
	void End();

	/// <summary>
	/// 画面への描画
	/// </summary>
	void Draw();

	/// <summary>
	/// 終了
	/// </summary>
	void Finalize();

private:

	/// <summary>
	/// デスクリプターヒープ生成
	/// </summary>
	void CreateDescriptorHeap();

	/// <summary>
	/// DirextX12初期化
	/// </summary>
	void InitialzeDX12();


	void CustomizeColor();

	/// <summary>
	/// エディターを変更
	/// </summary>
	void CustomizeEditor();





private:
	static ImGuiManager* instance;
	ImGuiManager(ImGuiManager&) = delete;
	ImGuiManager& operator = (ImGuiManager&) = delete;
private:
#ifdef _DEBUG
	// ポインタ
	DirectXCommon* dxCommon_ = nullptr;
	WinApp* winApp_ = nullptr;
	// SRV用デスクリプターヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
#endif

};

