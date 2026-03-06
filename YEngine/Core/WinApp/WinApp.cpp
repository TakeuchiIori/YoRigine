#include "WinApp.h"
#define IDI_ICON1 101

WinApp* WinApp::instance = nullptr;

#ifdef USE_IMGUI
#include <imgui_impl_win32.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
);
#endif


/// <summary>
/// Win32 のメッセージ処理
/// </summary>
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef USE_IMGUI
	// ImGui の入力処理
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return TRUE;
	}
#endif

	//-----------------------------------------
	// ゲーム固有のメッセージ処理
	//-----------------------------------------
	switch (msg)
	{
		// ウィンドウが破棄された
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	//-----------------------------------------
	// デフォルトのメッセージ処理
	//-----------------------------------------
	return DefWindowProc(hwnd, msg, wparam, lparam);
}


/// <summary>
/// WinApp シングルトン取得
/// </summary>
WinApp* WinApp::GetInstance()
{
	if (instance == nullptr) {
		instance = new WinApp;
	}
	return instance;
}


/// <summary>
/// Win32 ウィンドウの初期化
/// </summary>
void WinApp::Initialize()
{
	//-----------------------------------------
	// COM ライブラリ初期化（WIC などで必須）
	//-----------------------------------------
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		MessageBox(nullptr, L"COMライブラリの初期化に失敗しました", L"エラー", MB_OK);
		return;
	}

	//-----------------------------------------
	// ウィンドウクラス登録
	//-----------------------------------------
	wc_.lpfnWndProc = WindowProc;					// ウィンドウプロシージャ
	wc_.lpszClassName = L"CG2WindowClass";			// クラス名
	wc_.hInstance = GetModuleHandle(nullptr);		// インスタンス
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc_.hIcon = LoadIcon(wc_.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	RegisterClass(&wc_);

	//-----------------------------------------
	// クライアント領域サイズからウィンドウサイズを算出
	//-----------------------------------------
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	//-----------------------------------------
	// ウィンドウ生成
	//-----------------------------------------
	hwnd_ = CreateWindow(
		wc_.lpszClassName,						// クラス名
		L"LE3B_17_タケウチ_イオリ_ゴルディン",		// タイトル
		WS_OVERLAPPEDWINDOW,					// ウィンドウスタイル
		CW_USEDEFAULT,							// X
		CW_USEDEFAULT,							// Y
		wrc.right - wrc.left,					// 横幅
		wrc.bottom - wrc.top,					// 高さ
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr
	);

	//-----------------------------------------
	// ウィンドウ表示
	//-----------------------------------------
	ShowWindow(hwnd_, SW_SHOW);

	//-----------------------------------------
	// システムタイマー精度を向上
	//-----------------------------------------
	timeBeginPeriod(1);

	//-----------------------------------------
	// コンソール側のアイコン設定
	//-----------------------------------------
	HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	SendMessage(GetConsoleWindow(), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
}


/// <summary>
/// Win32 ウィンドウの終了処理
/// </summary>
void WinApp::Finalize()
{
	CloseWindow(hwnd_);
	UnregisterClass(wc_.lpszClassName, wc_.hInstance);
	CoUninitialize();

	delete instance;
	instance = nullptr;
}


/// <summary>
/// メッセージ処理
/// </summary>
bool WinApp::ProcessMessage()
{
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT) {
			return true;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return false;
}
