#include "TitleScene.h"
// Engine
#include "CoreScenes./Manager./SceneManager.h"
#include "Systems./Input./Input.h"
#include "Loaders./Texture./TextureManager.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"

#ifdef _DEBUG
#include "imgui.h"
#endif // DEBUG
#include "LightManager/LightManager.h"
#include "Sprite/SpriteCommon.h"
/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize()
{
    // カメラの生成
    currentCamera_ = cameraManager_.AddCamera();
    Object3dCommon::GetInstance()->SetDefaultCamera(currentCamera_.get());

    // 初期カメラモード設定
    cameraMode_ = CameraMode::FOLLOW;


    // 各オブジェクトの初期化
    //player_ = std::make_unique<Player>();
    //player_->Initialize(currentCamera_.get());

    sprite_ = std::make_unique<Sprite>();
    sprite_->Initialize("Resources/Textures/KoboTitle.png");
    sprite_->SetSize(Vector2{ 1280.0f,720.0f });
    sprite_->SetTextureSize(Vector2{ 1280,720 });

    // オーディオファイルのロード（例: MP3）
    soundData = Audio::GetInstance()->LoadAudio(L"Resources./images./harpohikunezumi.mp3");

    //// オーディオの再生
    //sourceVoice = Audio::GetInstance()->SoundPlayAudio(soundData);

    //// 音量の設定（0.0f ～ 1.0f）
    //Audio::GetInstance()->SetVolume(sourceVoice, 0.05f); // 80%の音量に設定


}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update()
{
    sprite_->Update();
    // シーン遷移中は入力を受け付けない
    //if (sceneManager_->IsTransitioning()) {
    //    return;
    //}
    if (Input::GetInstance()->PushKey(DIK_SPACE)) {
        sceneManager_->ChangeScene("Game");
    }

 


}


/// <summary>
/// 描画処理
/// </summary>
void TitleScene::Draw()
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
    LightManager::GetInstance()->SetCommandList();
    /// <summary>
    /// ここから描画可能です
    /// </summary>

   // player_->Draw();


#pragma endregion


}

void TitleScene::DrawOffScreen()
{


}

/// <summary>
/// 解放処理
/// </summary>
void TitleScene::Finalize()
{
    cameraManager_.RemoveCamera(currentCamera_);
}


void TitleScene::UpdateCameraMode()
{
#ifdef _DEBUG
    ImGui::Begin("Camera Mode");
    if (ImGui::Button("DEFAULT Camera")) {
        cameraMode_ = CameraMode::DEFAULT;
    }
    if (ImGui::Button("Follow Camera")) {
        cameraMode_ = CameraMode::FOLLOW;
    }
    if (ImGui::Button("Top-Down Camera")) {
        cameraMode_ = CameraMode::TOP_DOWN;
    }
    if (ImGui::Button("FPS Camera")) {
        cameraMode_ = CameraMode::FPS;
    }
    ImGui::End();
#endif
}

void TitleScene::UpdateCamera()
{
    switch (cameraMode_)
    {
    case CameraMode::DEFAULT:
    {
        currentCamera_->DefaultCamera();
    }
    break;
    case CameraMode::FOLLOW:
    {

    }
    break;
    case CameraMode::TOP_DOWN:
    {

    }
    break;
    case CameraMode::FPS:
    {

    }
    break;

    default:
        break;
    }
}



