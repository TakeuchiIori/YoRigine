#pragma once

// DLLエクスポート/インポートの制御
#ifdef GAME_BUILD_DLL
#define GAME_API __declspec(dllexport)
#else
#define GAME_API __declspec(dllimport)
#endif
#include "Framework/Framework.h" 

extern "C" {
	// Framework* インスタンスを生成して返す関数
	GAME_API Framework* CreateGame();

	// Framework* インスタンスを破棄する関数
	GAME_API void DestroyGame(Framework* pGame);
}