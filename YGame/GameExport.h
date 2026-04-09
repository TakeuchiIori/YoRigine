#pragma once
#include "GameAPI.h"
#include "Framework/Framework.h"

extern "C" {
    // Framework* インスタンスを生成して返す関数
    GAME_API Framework* CreateGame();

    // Framework* インスタンスを破棄する関数
    GAME_API void DestroyGame(Framework* pGame);
}