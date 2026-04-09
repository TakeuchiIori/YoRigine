#pragma once

#ifdef GAME_BUILD_DLL
// DLL ビルド時 (Debug) : エクスポート
#define GAME_API __declspec(dllexport)
#elif defined(GAME_IMPORT_DLL)
// DLL 使用側 (Debug の YMain) : インポート
#define GAME_API __declspec(dllimport)
#else
// StaticLib ビルド時 (Release) : 何もしない
#define GAME_API
#endif