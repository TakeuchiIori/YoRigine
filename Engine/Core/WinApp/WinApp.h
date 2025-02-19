#pragma once
#include "Windows.h"

// C++
#include <cstdint>

// Engine
#include "Debugger./LeakChecker.h"

class WinApp
{
public: // 静的メンバ関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
		WPARAM wparam, LPARAM lparam);
public:

	/// <summary>
	/// インスタンス取得
	/// </summary>
	/// <returns></returns>
	static WinApp* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了
	/// </summary>
	void Finalize();

	/// <summary>
	/// メッセージの処理
	/// </summary>
	bool ProcessMessage();

public: // アクセッサ
	HINSTANCE Gethinstance() { return wc.hInstance; }
	HWND GetHwnd() { return hwnd; }

public: // 定数
	// クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:
	static WinApp* instance;
	static HWND hwndImgui;
	
	WinApp() = default; 
	~WinApp() = default;
	WinApp(const WinApp&) = delete;
	WinApp& operator=(const WinApp&) = delete;
	// ウィンドウクラスの設定
	WNDCLASS wc{};
	// ウィンドウハンドル
	HWND hwnd;

};

