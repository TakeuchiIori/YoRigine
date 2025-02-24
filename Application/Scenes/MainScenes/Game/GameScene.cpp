#include "GameScene.h"
// Engine
#include "CoreScenes./Manager./SceneManager.h"
#include "Systems./Input./Input.h"
#include "Systems/Audio/Audio.h"
#include "Loaders./Texture./TextureManager.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "PipelineManager/SkinningManager.h"
#include "Loaders/Model/Model.h"
#include "Collision/CollisionManager.h"
#include <Systems/GameTime/HitStop.h>
#include <cstdlib>
#include <ctime>

#ifdef _DEBUG
#include "imgui.h"
#endif // DEBUG
#include "LightManager/LightManager.h"
#include "Sprite/SpriteCommon.h"
#include <Systems/GameTime/GameTIme.h>

/// <summary>
/// 初期化処理
/// </summary>
void GameScene::Initialize()
{
    srand(static_cast<unsigned int>(time(nullptr))); // 乱数シード設定
    // カメラの生成
    sceneCamera_ = cameraManager_.AddCamera();
    

    CollisionManager::GetInstance()->Initialize();
    // 線
    line_ = std::make_unique<Line>();
    line_->Initialize();
    line_->SetCamera(sceneCamera_.get());

    boneLine_ = std::make_unique<Line>();
    boneLine_->Initialize();
    boneLine_->SetCamera(sceneCamera_.get());


    //// コマンドパターン
    //inputHandler_ = std::make_unique<InputHandleMove>();

    //inputHandler_->AssignMoveFrontCommandPressKeyW();
    //inputHandler_->AssignMoveBehindCommandPressKeyS();
    //inputHandler_->AssignMoveRightCommandPressKeyD();
    //inputHandler_->AssignMoveLeftCommandPressKeyA();

	GameTime::GetInstance()->Initialize();
    
	followCamera_.Initialize();
    // 各オブジェクトの初期化
    player_ = std::make_unique<Player>();
    player_->Initialize(sceneCamera_.get());
    followCamera_.SetTarget(player_.get()->GetWorldTransform());
    
    // 地面
    ground_ = std::make_unique<Ground>();
    ground_->Initialize(sceneCamera_.get());
    
    // test
    test_ = std::make_unique<Object3d>();
    test_->Initialize();
    test_->SetModel("walk.gltf",true);
    //test_->SetModel("sneakWalk.gltf", true);
    testWorldTransform_.Initialize();

	offScreen_ = std::make_unique<OffScreen>();
	offScreen_->Initialize();
   

    // 初期カメラモード設定
    cameraMode_ = CameraMode::FOLLOW;

    // パーティクル
   
  
    emitterPosition_ = Vector3{ 0.0f, 0.0f, 0.0f }; // エミッタの初期位置
    particleCount_ = 1;
    particleEmitter_[0] = std::make_unique<ParticleEmitter>("Circle", emitterPosition_, particleCount_);
    particleEmitter_[0]->Initialize();



    //// オーディオファイルのロード（例: MP3）
    //soundData = Audio::GetInstance()->LoadAudio(L"Resources./images./harpohikunezumi.mp3");
    //// オーディオの再生
    //sourceVoice = Audio::GetInstance()->SoundPlayAudio(soundData);
    //// 音量の設定（0.0f ～ 1.0f）
    //Audio::GetInstance()->SetVolume(sourceVoice, 0.8f); // 80%の音量に設定


    sprite_ = std::make_unique<Sprite>();
    sprite_->Initialize("Resources/Textures/KoboRB.png");
    sprite_->SetSize(Vector2{ 1280.0f,720.0f });
    sprite_->SetTextureSize(Vector2{ 1280,720 });

    ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());

}

/// <summary>
/// 更新処理
/// </summary>
void GameScene::Update()
{


    //particleEmitter_[0]->SetPosition(player_->GetPosition());


    //iCommand_ = inputHandler_->HandleInput();

    //if (this->iCommand_) {
    //    iCommand_->Exec(*player_.get());
    //}

           // パーティクル更新

    CheckAllCollisions();
    CollisionManager::GetInstance()->UpdateWorldTransform();
    // スポーンタイマーを更新

    // objの更新
    player_->Update();
    player_->JsonImGui();
    //followCamera_.JsonImGui();

    // enemy_->Update();
    ground_->Update();
    test_->UpdateAnimation();


    particleEmitter_[0]->Emit();

    ParticleManager::GetInstance()->Update();
    // カメラ更新
    UpdateCameraMode();
    UpdateCamera();




    // ParticleManager::GetInstance()->UpdateParticlePlayerWeapon(weaponPos);
    ShowImGui();


    // particleEmitter_[1]->Update();

    JsonManager::ImGuiManager();
    // ワールドトランスフォーム更新
    testWorldTransform_.UpdateMatrix();
    cameraManager_.UpdateAllCameras();

    //=====================================================//
    /*                  これより下は触るな危険　　　　　　　   　*/
    //=====================================================//

    // ライティング
    LightManager::GetInstance()->ShowLightingEditor();


	sprite_->Update();
  
   
}


/// <summary>
/// 描画処理
/// </summary>
void GameScene::Draw()
{

#pragma region 3Dオブジェクト描画
    Object3dCommon::GetInstance()->DrawPreference();
    LightManager::GetInstance()->SetCommandList();
    /// <summary>
    /// ここから描画可能です
    /// </summary>
    CollisionManager::GetInstance()->Draw();
    player_->Draw();

    ground_->Draw();
    //line_->UpdateVertices(start_, end_);
  
    //line_->DrawLine();
   
#pragma endregion

#pragma region 骨付きアニメーション描画
    SkinningManager::GetInstance()->DrawPreference();
    LightManager::GetInstance()->SetCommandList();
    /// <summary>
    /// ここから描画可能です
    /// </summary>

    test_->Draw(sceneCamera_.get(), testWorldTransform_);

    // 骨描画
    if (test_ && test_->GetModel()->GetSkeleton().joints.size() > 0) {
        test_->DrawSkeleton(test_->GetModel()->GetSkeleton(), *boneLine_);
        boneLine_->DrawLine();
    }

   

#pragma endregion

#pragma region 演出描画
   
ParticleManager::GetInstance()->Draw();


#pragma endregion


#pragma region 2Dスプライト描画
    SpriteCommon::GetInstance()->DrawPreference();
    /// <summary>
    /// ここから描画可能です
    /// </summary>
    //sprite_->Draw();

#pragma endregion

	offScreen_->Draw();
    
}

/// <summary>
/// 解放処理
/// </summary>
void GameScene::Finalize()
{
    cameraManager_.RemoveCamera(sceneCamera_);
}


void GameScene::UpdateCameraMode()
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

void GameScene::UpdateCamera()
{
    switch (cameraMode_)
    {
    case CameraMode::DEFAULT:
    {
        sceneCamera_->DefaultCamera();
    }
    break;
    case CameraMode::FOLLOW:
    {
        
        followCamera_.Update();
		sceneCamera_->viewMatrix_ = followCamera_.matView_;
		sceneCamera_->transform_.translate = followCamera_.translate_;
		sceneCamera_->transform_.rotate = followCamera_.rotate_;

		sceneCamera_->UpdateMatrix();
    }
    break;
    case CameraMode::TOP_DOWN:
    {
        //Vector3 topDownPosition = Vector3(0.0f, 100.0f, 0.0f);
        //sceneCamera_->SetTopDownCamera(topDownPosition + player_->GetPosition());
        
        topDownCamera_.SetTarget(player_.get()->GetWorldTransform());
        topDownCamera_.Update();
		sceneCamera_->viewMatrix_ = topDownCamera_.matView_;
		sceneCamera_->transform_.translate = topDownCamera_.translate_;
		sceneCamera_->transform_.rotate = topDownCamera_.rotate_;

		sceneCamera_->UpdateMatrix();
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

void GameScene::ShowImGui()
{
#ifdef _DEBUG
    ImGui::Begin("FPS");
    ImGui::Text("FPS:%.1f", ImGui::GetIO().Framerate);
    ImGui::End();
    ImGui::Begin("Emitter");
    ImGui::DragFloat3("Emitter Position", &emitterPosition_.x, 0.1f);
   // particleEmitter_[0]->SetPosition(emitterPosition_);

    // パーティクル数の表示と調整
    ImGui::Text("Particle Count: %.0d", particleCount_); // 現在のパーティクル数を表示
    if (ImGui::Button("Increase Count")) {
        particleCount_ += 1; // パーティクル数を増加
    }
    if (ImGui::Button("Decrease Count")) {
        if (particleCount_ > 1) { // パーティクル数が1未満にならないように制限
            particleCount_ -= 1;
        }
    }
    //particleEmitter_[0]->SetCount(particleCount_);


    ImGui::End();
#endif // _DEBUG
}

void GameScene::CheckAllCollisions() {

    // 衝突マネージャーのリセット
    CollisionManager::GetInstance()->Reset();

    // コライダーをリストに登録
    CollisionManager::GetInstance()->AddCollider(player_.get());

    // コライダーリストに登録
    CollisionManager::GetInstance()->AddCollider(player_->GetPlayerWeapon());


   // CollisionManager::GetInstance()->AddCollider(enemy_.get());

    // 衝突判定と応答
    CollisionManager::GetInstance()->CheckAllCollisions();

}
