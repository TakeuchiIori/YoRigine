#include "TitleScene.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Systems./Input./Input.h"
#include "Loaders./Texture./TextureManager.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Sprite/SpriteCommon.h"
#include <Editor/Editor.h>
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>
#include <Collision/Core/CollisionManager.h>
#include <Systems/GameTime/GameTime.h>
#include <ModelManipulator/ModelManipulator.h>
#include "OffScreen/PostEffectManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize() {
	sceneCamera_ = cameraManager_.AddCamera();

	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initialize();
	YoRigine::JsonManager::SetCurrentScene("TitleScene");
	YoRigine::CollisionManager::GetInstance()->Initialize();
	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->LoadScene("TitleScene");
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());

	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------
	splineCamera_.Initialize();
	topDownCamera_.Initialize();
	defaultCamera_.Initialize();
	cameraMode_ = CameraMode::DEFAULT;

	//------------------------------------------------------------
	// タイトル専用要素の初期化
	//------------------------------------------------------------
	titleUI_ = std::make_unique<TitleUI>();
	titleUI_->Initialize();

	player_ = std::make_unique<DemoPlayer>();
	player_->Initialize(sceneCamera_.get());
	player_->SetMotion("Idle1");

	defaultCamera_.SetTarget(player_->GetWT());
	defaultCamera_.enableOrbit_ = true; // カメラ回転有効

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(sceneCamera_.get(), "Resources/DDS/vz_sinister_land_cubemap_ue.dds");

	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_.get());

	//------------------------------------------------------------
	// エディター登録（デバッグ用）
	//------------------------------------------------------------
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラモード", [this]() { UpdateCameraMode(); }, "Title");
	Editor::GetInstance()->RegisterGameUI("ライティング", [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); }, "Title");
#endif
}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update() {
	YoRigine::GameTime::Update();
	UpdateCamera();

#ifdef _DEBUG
	// デバッグカメラ切り替え
	if (YoRigine::Input::GetInstance()->TriggerKey(DIK_LCONTROL) ||
		YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::R_Stick)) {
		isDebugCamera_ = !isDebugCamera_;
	}
#endif

	// スペースまたはAボタンでゲームシーンへ遷移
	if (YoRigine::Input::GetInstance()->PushKey(DIK_SPACE) ||
		YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::A)) {
		sceneManager_->ChangeScene("Game");
	}

	// 各種更新
	player_->Update();
	skyBox_->Update();
	ground_->Update();

	// タイトル用パーティクル発生
	YoRigine::ParticleManager::GetInstance()->Emit("TitleParticle", Vector3(0, 3, 0), 10);

	YoRigine::ModelManipulator::GetInstance()->Update();
	cameraManager_.UpdateAllCameras();
	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
	YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(sceneCamera_.get());
	titleUI_->Update();
}

/// <summary>
/// 描画処理
/// </summary>
void TitleScene::Draw() {
	//------------------------------------------------------------
	// 3D描画（背景・キャラ）
	//------------------------------------------------------------
	skyBox_->Draw();
	Object3dCommon::GetInstance()->DrawPreference();
	DrawObject();
	YoRigine::ModelManipulator::GetInstance()->Draw();

	//------------------------------------------------------------
	// パーティクル描画
	//------------------------------------------------------------
	YoRigine::ParticleManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// 2D UI描画
	//------------------------------------------------------------
	SpriteCommon::GetInstance()->DrawPreference();
	titleUI_->Draw();
}

/// <summary>
/// オフスクリーン外の描画処理（現状未使用）
/// </summary>
void TitleScene::DrawNonOffscreen() {}

/// <summary>
/// 影の描画
/// </summary>
void TitleScene::DrawShadow()
{
	player_->DrawShadow();
	YoRigine::ModelManipulator::GetInstance()->DrawShadow();
}

/// <summary>
/// 解放処理
/// </summary>
void TitleScene::Finalize() {
	YoRigine::JsonManager::ClearSceneInstances("TitleScene");
	cameraManager_.RemoveCamera(sceneCamera_);
}

/// <summary>
/// オブジェクト描画（地面・プレイヤー）
/// </summary>
void TitleScene::DrawObject() {
	ground_->Draw();
	player_->Draw();
	player_->DrawAnimation();
}

/// <summary>
/// ライン描画（デバッグ用）
/// </summary>
void TitleScene::DrawLine() {}

/// <summary>
/// UI描画（必要に応じて拡張）
/// </summary>
void TitleScene::DrawUI() {}

/// <summary>
/// カメラモード切り替えUI（ImGui）
/// </summary>
void TitleScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	ImGui::Begin("Camera Mode");
	if (ImGui::Button("DEFAULT Camera")) cameraMode_ = CameraMode::DEFAULT;
	if (ImGui::Button("Follow Camera")) cameraMode_ = CameraMode::FOLLOW;
	if (ImGui::Button("Top-Down Camera")) cameraMode_ = CameraMode::TOP_DOWN;
	if (ImGui::Button("Debug Camera")) cameraMode_ = CameraMode::DEBUG;
	ImGui::End();
#endif
}

/// <summary>
/// カメラ更新処理
/// </summary>
void TitleScene::UpdateCamera() {
	switch (cameraMode_) {
	case CameraMode::DEFAULT:
		sceneCamera_->SetFovY(defaultCamera_.GetFov());
		defaultCamera_.Update();
		sceneCamera_->viewMatrix_ = defaultCamera_.matView_;
		sceneCamera_->transform_.translate = defaultCamera_.translate_;
		sceneCamera_->transform_.rotate = defaultCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::TOP_DOWN:
		if (player_) topDownCamera_.SetTarget(player_->GetWT());
		topDownCamera_.Update();
		sceneCamera_->viewMatrix_ = topDownCamera_.matView_;
		sceneCamera_->transform_.translate = topDownCamera_.translate_;
		sceneCamera_->transform_.rotate = topDownCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::SPLINE:
		if (player_) splineCamera_.SetTarget(player_->GetWT());
		splineCamera_.RegisterControlPoints();
		splineCamera_.Update();
		sceneCamera_->viewMatrix_ = splineCamera_.matView_;
		sceneCamera_->transform_.translate = splineCamera_.translate_;
		sceneCamera_->transform_.rotate = splineCamera_.rotate_;
		sceneCamera_->UpdateMatrix();
		break;

	case CameraMode::DEBUG:
		break;
	}
}