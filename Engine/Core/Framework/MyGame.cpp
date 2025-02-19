#include "MyGame.h"
#include "Particle./ParticleManager.h"

void MyGame::Initialize()
{
	// 基盤の初期化
	Framework::Initialize();

	// シーンファクトリを生成し、 シーンマネージャに最初のシーンをセット
	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->SetTransitionFactory(std::make_unique<FadeTransitionFactory>());
	SceneManager::GetInstance()->Initialize();
#ifdef _DEBUG
	SceneManager::GetInstance()->ChangeScene("Game");
#else
	SceneManager::GetInstance()->ChangeScene("Title");
#endif
	// パーティクルマネージャ生成
	ParticleManager::GetInstance()->Initialize(srvManager_);
	ParticleManager::GetInstance()->CreateParticleGroup("Enemy", "Resources/images/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("W", "Resources/images/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/images/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Player", "Resources/images/circle.png");
}

void MyGame::Finalize()
{
	Framework::Finalize();
}

void MyGame::Update()
{
	
	// 基盤の更新
	Framework::Update();

}

void MyGame::Draw()
{
	// DirectXの描画準備
	dxCommon_->PreDraw();
	// Srvの描画準備
	srvManager_->PreDraw();

	// ゲームの描画
	SceneManager::GetInstance()->Draw();


	//imguiManager_->Draw();

	// DirectXの描画終了
	dxCommon_->PostDraw();
}
