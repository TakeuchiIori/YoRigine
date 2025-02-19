#pragma once

#include "AbstractSceneFactory.h"
#include "CoreScenes/Base/BaseScene.h"


// このゲーム用のシーン工場
class SceneFactory : public AbstractSceneFactory
{
public:
	/// <summary>
	/// シーン生成
	/// </summary>
	/// <param name="sceneName"></param>
	/// <returns></returns>
	BaseScene* CreateScene(const std::string& sceneName) override;
};

