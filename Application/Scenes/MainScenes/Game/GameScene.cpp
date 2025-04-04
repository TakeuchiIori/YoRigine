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
#include "Collision/Core/CollisionManager.h"
#include <Systems/GameTime/HitStop.h>
#include "../Graphics/Culling/OcclusionCullingManager.h"

//C++
#include <cstdlib>
#include <ctime>
#include <numbers>

#ifdef _DEBUG
#include "imgui.h"
#endif // DEBUG
#include "LightManager/LightManager.h"
#include "Sprite/SpriteCommon.h"
#include <Systems/GameTime/GameTIme.h>
#include "Quaternion.h"
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

	mpInfo_ = std::make_unique<MapChipInfo>();
	mpInfo_->Initialize();
	mpInfo_->SetCamera(sceneCamera_.get());

	followCamera_.Initialize();
	debugCamera_.Initialize();
	// 各オブジェクトの初期化
	player_ = std::make_unique<Player>(mpInfo_->GetMapChipField());
	player_->Initialize(sceneCamera_.get());
	Vector3 playerPosition = mpInfo_->GetMapChipField()->GetMapChipPositionByIndex(1, 16);
	player_->SetPosition(playerPosition);
	followCamera_.SetTarget(player_.get()->GetWorldTransform());

	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(sceneCamera_.get());
	enemyManager_->SetPlayer(player_.get());

	// 地面
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_.get());

	// test
	test_ = std::make_unique<Object3d>();
	test_->Initialize();
	test_->SetModel("walk.gltf", true);
	//test_->SetModel("sneakWalk.gltf", true);
	testWorldTransform_.Initialize();




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

	

	uiBase_ = std::make_unique<UIBase>("UIButton");
	uiBase_->Initialize("Resources/JSON/UI/Button.json");

	uiSub_ = std::make_unique<UIBase>("UISub");
	uiSub_->Initialize("Resources/JSON/UI/Sub.json");

	obb.center = { 0.0f, 2.0f, 0.0f },
	obb.size = { 1.0f, 2.0f, 0.5f },
	obb.rotation = { 0.0f,0.0f,0.0f};

	OcclusionCullingManager::GetInstance()->Initialize();
}

/// <summary>
/// 更新処理
/// </summary>
void GameScene::Update()
{
	if (Input::GetInstance()->TriggerKey(DIK_LCONTROL) || Input::GetInstance()->IsPadTriggered(0, GamePadButton::RT)) {
		isDebugCamera_ = !isDebugCamera_;  // 現在の状態を反転
	}


	//particleEmitter_[0]->SetPosition(player_->GetPosition());


	//iCommand_ = inputHandler_->HandleInput();

	//if (this->iCommand_) {
	//    iCommand_->Exec(*player_.get());
	//}

	// パーティクル更新
	mpInfo_->Update();


	// objの更新
	if (!isDebugCamera_) {
	player_->Update();
	}
	enemyManager_->Update();
	

	ground_->Update();
	test_->UpdateAnimation();


	particleEmitter_[0]->Emit();

	ParticleManager::GetInstance()->Update();
	// カメラ更新
	UpdateCameraMode();
	UpdateCamera();


	ShowImGui();


	// particleEmitter_[1]->Update();

	JsonManager::ImGuiManager();
	// ワールドトランスフォーム更新
	testWorldTransform_.UpdateMatrix();
	cameraManager_.UpdateAllCameras();
	CollisionManager::GetInstance()->Update();

	//=====================================================//
	/*                  これより下は触るな危険　　　　　　　   　*/
	//=====================================================//

	// ライティング
	LightManager::GetInstance()->ShowLightingEditor();
	

	sprite_->Update();
	uiBase_->Update();

	uiSub_->Update();

}


/// <summary>
/// 描画処理
/// </summary>
void GameScene::Draw()
{
	//---------
	// 3D
	//---------
	Object3dCommon::GetInstance()->DrawPreference();
	LightManager::GetInstance()->SetCommandList();
	DrawObject();


	//---------
	// Animation
	//---------
	SkinningManager::GetInstance()->DrawPreference();
	LightManager::GetInstance()->SetCommandList();
	DrawAnimation();
	



	OcclusionCullingManager::GetInstance()->ResolvedOcclusionQuery();

}

void GameScene::DrawOffScreen()
{
	//----------
	// Particle
	//----------
	ParticleManager::GetInstance()->Draw();

	//----------
	// Sprite
	//----------
	SpriteCommon::GetInstance()->DrawPreference();
	DrawSprite();

	//----------
	// Line
	//----------
	DrawLine();

}

void GameScene::DrawObject()
{
	mpInfo_->Draw();
	player_->Draw();
	ground_->Draw();
	enemyManager_->Draw();


}

void GameScene::DrawSprite()
{
	//sprite_->Draw();
	uiBase_->Draw();
	uiSub_->Draw();
}

void GameScene::DrawAnimation()
{
	test_->Draw(sceneCamera_.get(), testWorldTransform_);
	
}

void GameScene::DrawLine()
{
	// 骨描画
	if (test_ && test_->GetModel()->GetSkeleton().joints.size() > 0) {
		test_->DrawSkeleton(*boneLine_);
		boneLine_->DrawLine();
	}
	player_->DrawCollision();
	//enemyManager_->DrawCollision();
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
	if (ImGui::Button("Debug Camera")) {
		cameraMode_ = CameraMode::DEBUG;
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
	case CameraMode::DEBUG:
	{
		if (isDebugCamera_) {
			sceneCamera_->SetFovY(debugCamera_.GetFov());
			debugCamera_.Update();
			sceneCamera_->viewMatrix_ = debugCamera_.matView_;
			sceneCamera_->transform_.translate = debugCamera_.translate_;
			sceneCamera_->transform_.rotate = debugCamera_.rotate_;
			sceneCamera_->UpdateMatrix();
		}
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
