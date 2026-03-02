#include <windows.h>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include "Framework/Framework.h"
#include "Debugger/Logger.h"

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

	// 初回ロード
	if (!LoadGameDLL()) {
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

		// ホットリロードチェック
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

	return 0;
}
