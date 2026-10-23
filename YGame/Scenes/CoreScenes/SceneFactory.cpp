#include "SceneFactory.h"
// Engine
#include "MainScenes/Clear/ClearScene.h"
#include "MainScenes/Game/GameScene.h"
#include "MainScenes/Title/TitleScene.h"

#include "SceneSystems/DevelopScene/DevelopScene.h"

// App
#include "GameIds.h"

/// <summary>
/// シーンを作成する
/// </summary>
/// <param name="シーンの名前"></param>
/// <returns></returns>
std::unique_ptr<BaseScene>
SceneFactory::CreateScene(const std::string &sceneName) {
  // 次のシーンを生成
  std::unique_ptr<BaseScene> newScene = nullptr;

  if (sceneName == SceneId::Title) {
    newScene = std::make_unique<TitleScene>();
  } else if (sceneName == SceneId::Game) {
    newScene = std::make_unique<GameScene>();
  } else if (sceneName == SceneId::Clear) {
    newScene = std::make_unique<ClearScene>();
  } else if (sceneName == SceneId::Develop) {
    newScene = std::make_unique<DevelopScene>();
  }

  return newScene;
}
