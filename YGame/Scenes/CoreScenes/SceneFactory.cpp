#include "SceneFactory.h"
// Engine
#include "MainScenes/Game/GameScene.h"
#include "MainScenes/Title/TitleScene.h"
#include "MainScenes/Clear/ClearScene.h"

#include "SceneSystems/DevelopScene/DevelopScene.h"

/// <summary>
/// シーンを作成する
/// </summary>
/// <param name="シーンの名前"></param>
/// <returns></returns>
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	// 次のシーンを生成
	std::unique_ptr<BaseScene> newScene = nullptr;

	if (sceneName == "Title") {
		newScene = std::make_unique<TitleScene>();
	} else if (sceneName == "Game") {
		newScene = std::make_unique<GameScene>();
	} else if (sceneName == "Clear") {
		newScene = std::make_unique<ClearScene>();
	}
	else if (sceneName == "Develop") {
		newScene = std::make_unique<DevelopScene>();
	}

	return newScene;
}
