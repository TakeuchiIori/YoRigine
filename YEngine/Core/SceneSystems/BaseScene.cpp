#include "BaseScene.h"
#include <Object3D/Object3dCommon.h>

void BaseScene::InitializeCommon()
{
	YoRigine::GameTime::Initialize();
	YoRigine::CollisionManager::GetInstance()->Initialize();

	// パーティクル
	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());

	// モデルマニピュレータ
	YoRigine::ModelManipulator::GetInstance()->LoadScene(sceneName_);
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());
}

void BaseScene::UpdateCommon()
{
	YoRigine::GameTime::Update();
	YoRigine::ModelManipulator::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(sceneCamera_.get());
}

void BaseScene::DrawCommonObject()
{
	Object3dCommon::GetInstance()->DrawPreference();
	YoRigine::LightManager::GetInstance()->SetCommandList();
}

void BaseScene::DrawCommonParticles()
{
	YoRigine::ParticleManager::GetInstance()->Draw();
}
