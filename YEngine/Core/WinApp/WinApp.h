#pragma once
#include "Windows.h"

// C++
#include <cstdint>
#include <string>

/// <summary>
/// ウィンドウクラス
/// </summary>
class WinApp {
public: // 静的メンバ関数
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

  // WM_CHAR
  // で溜まった文字列（UTF-16。BMP想定・IME確定後の完成文字がそのまま来る）を取り出し、
  // 内部バッファをクリアする。名前入力等のテキスト入力UIから毎フレーム呼ぶ想定。
  static std::wstring PopTextInput();

public:
  ///************************* 基本的な関数 *************************///

  static WinApp *GetInstance();
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
  static WinApp *instance;

  WinApp() = default;
  ~WinApp() = default;
  WinApp(const WinApp &) = delete;
  WinApp &operator=(const WinApp &) = delete;
  // ウィンドウクラスの設定
  WNDCLASS wc_{};
  // ウィンドウハンドル
  HWND hwnd_ = nullptr;

  // WM_CHAR で受け取った文字を溜めるバッファ（PopTextInput()
  // で取り出してクリアする）
  static std::wstring textInputBuffer_;
};
