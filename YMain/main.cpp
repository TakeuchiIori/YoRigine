#include "Debugger/Logger.h"
#include "Framework/Framework.h"
#include "GameExport.h"
#include <memory>
#include <windows.h>

//-----------------------------------------------------------------------------
// エントリーポイント
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

  // ゲームインスタンス生成（DLL境界のextern "C"ファクトリが返す生ポインタを
  // unique_ptrで受け、破棄は対になるDestroyGame()に委譲する）
  std::unique_ptr<Framework, void (*)(Framework *)> gameInstance(CreateGame(),
                                                                 DestroyGame);
  if (!gameInstance) {
    MessageBoxW(nullptr, L"ゲームの初期化に失敗しました", L"Error", MB_OK);
    return -1;
  }

  MSG msg = {};
  while (true) {

    // Windowメッセージ処理
    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT)
        break;

      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    // 終了リクエスト
    if (gameInstance->IsEndRequst())
      break;

    // 更新・描画
    gameInstance->Update();
    gameInstance->Draw();
  }

  return 0;
}