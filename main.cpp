#define _CRTDBG_MAP_ALLOC
#include "Framework./MyGame.h"
#include <crtdbg.h>

//Windowsアプリのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	//_CrtSetBreakAlloc(6327); // ←{1730} を例に


	std::unique_ptr<Framework> game = std::make_unique<MyGame>();

	game->Run();
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);


	return 0;// main関数のリターン
}

