#pragma once

#ifdef GAME_BUILD_DLL
// DLLを作るとき (GameRuntime) は「輸出」
#define GAME_API __declspec(dllexport)
#else
// DLLを使うとき (YGame) は「輸入」
#define GAME_API __declspec(dllimport)
#endif