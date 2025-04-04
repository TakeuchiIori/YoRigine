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
	offScreen_ = std::make_unique<OffScreen>();
	offScreen_->Initialize();
#ifdef _DEBUG
	SceneManager::GetInstance()->ChangeScene("Game");
#else
	SceneManager::GetInstance()->ChangeScene("Title");
#endif
	// パーティクルマネージャ生成
	ParticleManager::GetInstance()->Initialize(srvManager_);
	ParticleManager::GetInstance()->CreateParticleGroup("EnemyParticle", "Resources/images/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("W", "Resources/images/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "Resources/images/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("PlayerParticle", "Resources/images/circle.png");
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
	dxCommon_->PreDrawScene();
	// Srvの描画準備
	srvManager_->PreDraw();

	// ゲームの描画
	SceneManager::GetInstance()->Draw();

	dxCommon_->PreDrawImGui();
	offScreen_->SetProjection(SceneManager::GetInstance()->GetScene()->GetViewProjection());
	offScreen_->Draw();

	dxCommon_->DepthBarrier();
	SceneManager::GetInstance()->DrawForOffscreen();


	imguiManager_->Draw();
	// DirectXの描画終了
	dxCommon_->PostDraw();
}
