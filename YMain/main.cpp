#include <windows.h>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include "Framework/Framework.h"
#include "Debugger/Logger.h"

// ============================================================
// Live++ (Debug ビルドのみ有効)
// ============================================================
#ifdef _DEBUG
#include "x64/LPP_API_x64_CPP.h"
static lpp::LppDefaultAgent s_lppAgent;
#endif

//-----------------------------------------------------------------------------
// グローバル
//-----------------------------------------------------------------------------
HMODULE hGameDLL = nullptr;
Framework* gameInstance = nullptr;

typedef Framework* (*CreateGameFunc)();
typedef void (*DestroyGameFunc)(Framework*);

CreateGameFunc CreateGameFn = nullptr;
DestroyGameFunc DestroyGameFn = nullptr;

std::filesystem::file_time_type lastWriteTime;

//-----------------------------------------------------------------------------
// EXE のあるディレクトリを取得
//-----------------------------------------------------------------------------
std::filesystem::path GetExecutableDir() {
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	return std::filesystem::path(exePath).parent_path();
}

//-----------------------------------------------------------------------------
// DLL ロード処理（絶対パス対応）
//-----------------------------------------------------------------------------
bool LoadGameDLL() {

	auto exeDir = GetExecutableDir();
	auto dllOrigin = exeDir / "YGame.dll";
	auto dllHot = exeDir / "YGame_Hot.dll";

	Logger("--- Hot Reload Attempt Start ---");

	// 古いインスタンス破棄
	if (gameInstance) {
		Logger("DestroyGameFn...");
		DestroyGameFn(gameInstance);
		gameInstance = nullptr;
	}

	// 既存 DLL アンロード
	if (hGameDLL) {
		Logger("FreeLibrary...");
		FreeLibrary(hGameDLL);
		hGameDLL = nullptr;
	}

	// DLL の存在チェック
	if (!std::filesystem::exists(dllOrigin)) {
		Logger("Error: YGame.dll not found near EXE.");
		MessageBoxW(nullptr, L"YGame.dll が見つかりません。\nEXE と同じフォルダに置いてください。", L"Error", MB_OK);
		return false;
	}

	// 更新日時保存
	lastWriteTime = std::filesystem::last_write_time(dllOrigin);

	// DLL コピーして HotDLL にする
	try {
		Logger("Copy DLL...");
		std::filesystem::copy_file(dllOrigin, dllHot, std::filesystem::copy_options::overwrite_existing);
	}
	catch (...) {
		Logger("DLL copy failed.");
		return false;
	}

	// DLL ロード
	Logger("LoadLibraryW...");
	hGameDLL = LoadLibraryW(dllHot.c_str());
	if (!hGameDLL) {
		MessageBoxW(nullptr, L"Hot DLL のロードに失敗しました", L"Error", MB_OK);
		return false;
	}

	// ============================================================
	// Live++: ロードした DLL を Live++ に登録する
	// ============================================================
#ifdef _DEBUG
	if (lpp::LppIsValidDefaultAgent(&s_lppAgent)) {
		// DLL のパスを取得して Live++ に登録
		wchar_t dllFullPath[MAX_PATH];
		GetModuleFileNameW(hGameDLL, dllFullPath, MAX_PATH);
		s_lppAgent.EnableModule(
			dllFullPath,
			lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES,
			nullptr, nullptr
		);
		Logger("Live++: YGame_Hot.dll registered.");
	}
#endif

	// 関数アドレス取得
	CreateGameFn = (CreateGameFunc)GetProcAddress(hGameDLL, "CreateGame");
	DestroyGameFn = (DestroyGameFunc)GetProcAddress(hGameDLL, "DestroyGame");

	if (!CreateGameFn || !DestroyGameFn) {
		MessageBoxW(nullptr, L"CreateGame / DestroyGame が DLL にありません", L"Error", MB_OK);
		FreeLibrary(hGameDLL);
		return false;
	}

	// 新しいゲームインスタンス生成
	Logger("CreateGameFn()");
	gameInstance = CreateGameFn();

	Logger("DLL Load Complete.");
	return true;
}

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// ============================================================
	// Live++ 初期化 (Debug ビルドのみ)
	// ============================================================
#ifdef _DEBUG
	// LivePP フォルダは EXE の隣にある前提
	// (または Externals/LivePP への絶対パス を指定してもよい)
// 例: Externals/LivePP が プロジェクトルートにある場合
	s_lppAgent = lpp::LppCreateDefaultAgent(nullptr, L"Externals/LivePP");

	if (lpp::LppIsValidDefaultAgent(&s_lppAgent)) {
		// EXE 本体のパスを取得して Live++ に登録
		wchar_t exeFullPath[MAX_PATH];
		GetModuleFileNameW(nullptr, exeFullPath, MAX_PATH);
		s_lppAgent.EnableModule(
			exeFullPath,
			lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES,
			nullptr, nullptr
		);
		Logger("Live++ initialized.");
	}
	else {
		// Live++ が見つからなくても続行（任意で MessageBox に変えてもよい）
		Logger("Warning: Live++ agent could not be created. Continuing without Live++.");
	}
#endif

	// 初回ロード
	if (!LoadGameDLL()) {
#ifdef _DEBUG
		lpp::LppDestroyDefaultAgent(&s_lppAgent);
#endif
		return -1;
	}

	MSG msg = {};
	while (true) {

		// Window message handling
		if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		// ホットリロードチェック（DLL まるごと入れ替え）
		try {
			auto exeDir = GetExecutableDir();
			auto dllOrigin = exeDir / "YGame.dll";
			auto currentWriteTime = std::filesystem::last_write_time(dllOrigin);

			if (currentWriteTime != lastWriteTime) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				LoadGameDLL();
			}
		}
		catch (...) {}

		// Game update/draw
		if (gameInstance) {

			if (gameInstance->IsEndRequst())
				break;

			gameInstance->Update();
			gameInstance->Draw();
		}
	}

	if (gameInstance)
		DestroyGameFn(gameInstance);

	// ============================================================
	// Live++ 終了処理
	// ============================================================
#ifdef _DEBUG
	lpp::LppDestroyDefaultAgent(&s_lppAgent);
#endif

	return 0;
}