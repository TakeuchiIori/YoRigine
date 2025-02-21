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

	// メモリアロケータのカスタマイズ
	ImGui::SetAllocatorFunctions(
		[](size_t size, void* user_data) { return _aligned_malloc(size, 16); },
		[](void* ptr, void* user_data) { _aligned_free(ptr); },
		nullptr
	);

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

	ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;
	ImGui::Begin("MainDockSpace", nullptr, ImGuiWindowFlags_NoCollapse);
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
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, (void*)dxCommon_->GetCommandList().Get());
	}
	Draw();
#endif
}

void ImGuiManager::Draw()
{
#ifdef _DEBUG
	// デスクリプターヒープの配列をセットするコマンド
	ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_.Get()};
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
		DXGI_FORMAT_R8G8B8A8_UNORM, srvHeap_.Get(),
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

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		// DockSpace背景色を透明に設定
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);			// 完全に透明
		// パディングとスペーシングの調整
		style.WindowPadding = ImVec2(0.0f, 0.0f);									// ウィンドウ内の余白をゼロに

		// ここでカラーやスタイル設定を変更
		colors[ImGuiCol_WindowBg] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);					// 背景色 (ダークグレー)
		colors[ImGuiCol_TitleBg] = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);				// タイトルバー (暗い灰色)
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);			// アクティブなタイトルバー (少し明るい灰色)
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.4f, 0.4f, 0.42f, 1.0f);		// 折りたたまれたタイトルバー
		colors[ImGuiCol_Button] = ImVec4(0.5f, 0.52f, 0.6f, 1.0f);					// ボタン (青灰色)
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.6f, 0.65f, 0.75f, 1.0f);			// ホバーしたボタン (明るい青灰色)
		colors[ImGuiCol_ButtonActive] = ImVec4(0.55f, 0.58f, 0.65f, 1.0f);			// 押されたボタン (少し暗めの青灰色)
		colors[ImGuiCol_FrameBg] = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);				// 入力欄やフレーム背景
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);			// ホバーしたフレーム背景
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.55f, 0.55f, 0.6f, 1.0f);			// アクティブなフレーム背景
		colors[ImGuiCol_CheckMark] = ImVec4(0.56f, 0.9f, 1.15f, 1.0f);				// チェックマーク (青アクセント)
		colors[ImGuiCol_SliderGrab] = ImVec4(0.56f, 0.9f, 1.15f, 1.0f);				// スライダーつまみ (青アクセント)
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.67f, 1.0f, 1.25f, 1.0f);		// アクティブなスライダーつまみ
		colors[ImGuiCol_Header] = ImVec4(0.5f, 0.55f, 0.6f, 1.0f);					// ヘッダー背景
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.6f, 0.65f, 0.7f, 1.0f);			// ホバーしたヘッダー
		colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.6f, 0.65f, 1.0f);			// アクティブなヘッダー
		colors[ImGuiCol_Tab] = ImVec4(0.45f, 0.48f, 0.52f, 1.0f);					// タブ背景
		colors[ImGuiCol_TabHovered] = ImVec4(0.55f, 0.6f, 0.7f, 1.0f);				// ホバーしたタブ
		colors[ImGuiCol_TabActive] = ImVec4(0.5f, 0.55f, 0.65f, 1.0f);				// アクティブなタブ
		colors[ImGuiCol_Separator] = ImVec4(0.55f, 0.55f, 0.6f, 1.0f);				// セパレーター
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.65f, 0.7f, 0.75f, 1.0f);		// ホバーしたセパレーター
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.75f, 0.8f, 0.85f, 1.0f);		// アクティブなセパレーター
		colors[ImGuiCol_Text] = ImVec4(1.2f, 1.2f, 1.2f, 1.0f);						// テキスト (白)
		colors[ImGuiCol_TextDisabled] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);				// 無効化されたテキスト (灰色)
		colors[ImGuiCol_Border] = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);					// 境界線
		colors[ImGuiCol_BorderShadow] = ImVec4(0.3f, 0.3f, 0.3f, 0.0f);				// 境界線の影
		ImVec4 inactiveTabColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);					// 選択していないタブの色
		ImVec4 inactiveTabActiveColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);				// 選択していないがアクティブなタブの色
		ImVec4 activeTabColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);						// 選択されたタブの色
		colors[ImGuiCol_TabUnfocused] = inactiveTabColor;							// 非アクティブなタブの色
		colors[ImGuiCol_TabUnfocusedActive] = inactiveTabActiveColor;
		
		// スタイルの変更
		style.WindowRounding = 15.0f;      // ウィンドウの角丸
		style.FrameRounding = 4.0f;        // フレームの角丸
		style.ScrollbarSize = 15.0f;       // スクロールバーのサイズ



	}
#endif // _DEBUG
}

void ImGuiManager::CustomizeEditor()
{
#ifdef _DEBUG
	// エディター同士をドッキング
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
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
	io.Fonts->AddFontFromFileTTF("Resources/Fonts/FiraMono-Medium.ttf", 14.0f, &config, ranges);
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
