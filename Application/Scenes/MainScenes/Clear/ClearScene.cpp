#include "ClearScene.h"

#include "CoreScenes./Manager./SceneManager.h"
#include "Systems./Input./Input.h"
#include "Loaders./Texture./TextureManager.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "Sprite/SpriteCommon.h"

void ClearScene::Initialize()
{
    sprite_ = std::make_unique<Sprite>();
    sprite_->Initialize("Resources/Textures/KoboClear.png");
    sprite_->SetSize(Vector2{ 1280.0f,720.0f });
    sprite_->SetTextureSize(Vector2{ 1280,720 });


}

void ClearScene::Finalize()
{
}

void ClearScene::Update()
{
    sprite_->Update();
    fade_->Update();
}

void ClearScene::Draw()
{
#pragma region 演出描画
    ParticleManager::GetInstance()->Draw();



#pragma endregion
#pragma region 2Dスプライト描画
    SpriteCommon::GetInstance()->DrawPreference();
    /// <summary>
    /// ここから描画可能です
    /// </summary>
    /// 

    sprite_->Draw();


#pragma endregion

#pragma region 3Dオブジェクト描画
    Object3dCommon::GetInstance()->DrawPreference();
    /// <summary>
    /// ここから描画可能です
    /// </summary>



#pragma endregion

}
