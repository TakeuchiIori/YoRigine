#include "SceneFactory.h"
// Engine
#include "MainScenes/Game/GameScene.h"
#include "MainScenes/Title/TitleScene.h"
#include "MainScenes/Clear/ClearScene.h"
//#include "MainScenes/Result/ResultScene.h"

BaseScene* SceneFactory::CreateScene(const std::string& sceneName)
{
    // 次のシーンを生成
    BaseScene* newScene = nullptr;

    if (sceneName == "Title") {
        newScene = new TitleScene();
    }
    else if (sceneName == "Game") {
        newScene = new GameScene();
    }
    else if (sceneName == "Clear") {
        newScene = new ClearScene();
    }

    return newScene;
}
