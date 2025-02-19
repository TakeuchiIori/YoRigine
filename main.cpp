#include "Framework./MyGame.h"


//Windowsアプリのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	std::unique_ptr<Framework> game = std::make_unique<MyGame>();

	game->Run();

	return 0;// main関数のリターン
}

