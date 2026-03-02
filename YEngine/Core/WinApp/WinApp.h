#pragma once
#include "Windows.h"

// C++
#include <cstdint>

/// <summary>
/// ウィンドウクラス
/// </summary>
class WinApp
{
public: // 静的メンバ関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
		WPARAM wparam, LPARAM lparam);
public:
	///************************* 基本的な関数 *************************///

	static WinApp* GetInstance();
	void Initialize();
	void Finalize();
	// メッセージの処理
	bool ProcessMessage();

public:
	///************************* アクセッサ *************************///
	HINSTANCE GethInstance() { return wc_.hInstance; }
	HWND GetHwnd() { return hwnd_; }

public:
	///************************* 定数 *************************///
	// クライアント領域のサイズ 16 : 9
	static const int32_t kClientWidth = 1600;
	static const int32_t kClientHeight = 900;


private:
	///************************* メンバ変数 *************************///
	static WinApp* instance;

	WinApp() = default;
	~WinApp() = default;
	WinApp(const WinApp&) = delete;
	WinApp& operator=(const WinApp&) = delete;
	// ウィンドウクラスの設定
	WNDCLASS wc_{};
	// ウィンドウハンドル
	HWND hwnd_ = nullptr;

};

