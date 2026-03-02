#pragma once
#include "CoreScenes/SceneFactory.h"
#include "SceneSystems/SceneManager.h"
#include "Framework/Framework.h"
#include "OffScreen/OffScreen.h"
#include <SceneSystems/AbstractSceneFactory.h>
#include "Loaders/Texture/TexturePreloader.h"

/// <summary>
/// ゲームのメインクラス
/// </summary>
class  MyGame : public Framework
{

public:

	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 更新
	void Update() override;

	// 描画
	void Draw() override;

private:
	OffScreen* offScreen_;
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
	TexturePreloader texturePreloader_;
};
