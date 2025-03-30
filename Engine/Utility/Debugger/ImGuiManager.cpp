#include "ImGuiManager.h"
// Engine
#include "WinApp./WinApp.h"
#include "DX./DirectXCommon.h"
#ifdef _DEBUG
#include "imgui.h"
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "imgui_internal.h"        
#include <imgui_impl_dx12.cpp>
#endif


ImGuiManager* ImGuiManager::instance = nullptr;
ImGuiManager* ImGuiManager::GetInstance()
{
	if (instance == nullptr) {
		instance = new ImGuiManager;
	}
	return instance;
}

void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
{
#ifdef _DEBUG
	// メンバ変数に引数を渡す
	dxCommon_ = dxCommon;

	winApp_ = winApp;

	// ImGuiのコンテキストを生成
	ImGui::CreateContext();

	// Editorの設定
	CustomizeEditor();

	// DockSpaceの設定
	CustomizeColor();

	// デスクリプタヒープ生成
	CreateDescriptorHeap();

	// DirectX12の初期化
	InitialzeDX12();

#endif
}

void ImGuiManager::Begin()
{
#ifdef _DEBUG
	// ImGuiフレーム開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f)); // ウィンドウ全体をカバーする

	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::SetNextWindowBgAlpha(0.0f); // 背景を透明にする
	ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::Begin("MainDockSpace", nullptr, dockspace_flags);

	// DockSpaceの作成
	ImGui::DockSpace(ImGui::GetID("MainDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
#endif
}

void ImGuiManager::End()
{
#ifdef _DEBUG
	ImGui::EndFrame();
	// 描画前準備
	ImGui::Render();
#endif
}

void ImGuiManager::Draw()
{
#ifdef _DEBUG
	// デスクリプターヒープの配列をセットするコマンド
	ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	if (ImGui::GetDrawData()->TotalVtxCount == 0) {
		printf("Warning: No vertices to render!\n");
		return;
	}
	// 描画コマンドを発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList().Get());

#endif
}

void ImGuiManager::CreateDescriptorHeap()
{
#ifdef _DEBUG
	// デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// デスクリプタヒープ生成
	HRESULT hr = dxCommon_->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap_));
	// ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr));
#endif // DEBUG
}

void ImGuiManager::InitialzeDX12()
{
#ifdef _DEBUG
	if (!srvHeap_) {
		printf("srvHeap_ is NULL! Check CreateDescriptorHeap!\n");
		assert(srvHeap_);
	}

	// DirectX12の初期化
	IMGUI_CHECKVERSION();
	// Win32用初期化
	ImGui_ImplWin32_Init(winApp_->GetHwnd());

	HRESULT hr = ImGui_ImplDX12_Init(
		dxCommon_->GetDevice().Get(),
		dxCommon_->GetBackBufferCount(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvHeap_.Get(),
		srvHeap_->GetCPUDescriptorHandleForHeapStart(),
		srvHeap_->GetGPUDescriptorHandleForHeapStart()
	);

	if (FAILED(hr)) {
		printf("ImGui_ImplDX12_Init failed! HRESULT: 0x%08X\n", hr);
		assert(SUCCEEDED(hr)); // ここで停止してエラー内容を確認
	}


	ImGui_ImplDX12_InitPlatformInterface();

#endif // DEBUG
}

void ImGuiManager::CustomizeColor()
{
#ifdef _DEBUG


	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	// パディングとスペーシングの調整
	style.WindowPadding = ImVec2(0.0f, 0.0f);									// ウィンドウ内の余白をゼロに

	// ここでカラーやスタイル設定を変更
	colors[ImGuiCol_TitleBg] = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.3f, 0.3f, 0.35f, 1.0f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.2f, 0.2f, 0.22f, 1.0f);
	colors[ImGuiCol_Button] = ImVec4(0.3f, 0.32f, 0.4f, 1.0f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.45f, 0.55f, 1.0f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.38f, 0.45f, 1.0f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.35f, 1.0f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.35f, 0.4f, 1.0f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.36f, 0.7f, 0.95f, 1.0f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.7f, 0.95f, 1.0f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.47f, 0.8f, 1.05f, 1.0f);
	colors[ImGuiCol_Header] = ImVec4(0.3f, 0.35f, 0.4f, 1.0f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.4f, 0.45f, 0.5f, 1.0f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.4f, 0.45f, 1.0f);
	colors[ImGuiCol_Tab] = ImVec4(0.35f, 0.28f, 0.32f, 0.9f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.4f, 0.5f, 1.0f);
	colors[ImGuiCol_TabActive] = ImVec4(0.3f, 0.35f, 0.45f, 1.0f);
	colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.4f, 1.0f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.45f, 0.5f, 0.55f, 1.0f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.55f, 0.6f, 0.65f, 1.0f);
	colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	colors[ImGuiCol_Border] = ImVec4(0.3f, 0.3f, 0.35f, 0.9f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.15f, 0.15f, 0.15f, 0.0f);
	ImVec4 inactiveTabColor = ImVec4(0.2f, 0.2f, 0.2f, 0.9f);
	ImVec4 inactiveTabActiveColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
	ImVec4 activeTabColor = ImVec4(0.4f, 0.4f, 0.4f, 0.9f);
	colors[ImGuiCol_TabUnfocused] = inactiveTabColor;
	colors[ImGuiCol_TabUnfocusedActive] = inactiveTabActiveColor;

	// スタイルの変更
	style.WindowRounding = 15.0f;
	style.FrameRounding = 4.0f;
	style.ScrollbarSize = 15.0f;

#endif // _DEBUG
}

void ImGuiManager::CustomizeEditor()
{
#ifdef _DEBUG
	// エディター同士をドッキング
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImFontConfig config;
	config.OversampleH = 1;
	config.OversampleV = 1;
	config.PixelSnapH = true;
	config.FontDataOwnedByAtlas = false;  // フォントデータの所有権を移譲しない

	// 必要な文字範囲のみロード
	static const ImWchar ranges[] = {
		0x0020, 0x00FF,  // Basic Latin
		0x3000, 0x30FF,  // CJK Symbols, Hiragana, Katakana
		0x4E00, 0x9FAF,  // CJK Ideographs
		0,
	};
	io.Fonts->AddFontFromFileTTF("Resources/Fonts/ipaexg.ttf", 14.0f, &config, ranges);
#endif;
}

void ImGuiManager::Finalize()
{
#ifdef _DEBUG
	// 後始末
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// デスクリプターヒープを解放
	if (srvHeap_) {
		srvHeap_.Reset();
	}

	delete instance;
	instance = nullptr;
#endif // DEBUG
}
