#pragma once

#include "SceneSystems/AbstractSceneFactory.h"
#include "SceneSystems/BaseScene.h"


// このゲーム用のシーン工場
class SceneFactory : public AbstractSceneFactory
{
public:
	/// <summary>
	/// シーン生成
	/// </summary>
	/// <param name="sceneName"></param>
	/// <returns></returns>
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
};

