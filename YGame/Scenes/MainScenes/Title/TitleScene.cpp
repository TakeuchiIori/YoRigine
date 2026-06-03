#include "TitleScene.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Systems./Input./Input.h"
#include "Loaders./Texture./TextureManager.h"
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
#include "Systems/Camera/Virtuals/TitleCamera/TitleCamera.h"
#include "Systems/Camera/CameraDirector.h"
#include "Particle/YEmitterGroupManager.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize() {

	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------

	// 出力用カメラの実体を生成
	sceneCamera_ = std::make_unique<Camera>();
	auto director = CameraDirector::GetInstance();
	director->Initialize();


	cameraEditor_ = std::make_unique<CameraEditor>();
	cameraEditor_->Initialize();
	cameraEditor_->SetFilePath("Resources/Json/VirtualCameraData/TitleScene.json");
	cameraEditor_->LoadFileOrDefault(cameraEditor_->GetFilePath(), "Title");
	cameraMode_ = CameraMode::TITLE;
	// カメラの登録
	auto titleCamera = director->GetCamera("TitleCamera");
	auto debug = director->GetCamera("MainDebug");

	// タイトル用のポストエフェクトの追加（TestGame: Fog + GodRays + Bloom 等）
	auto postEffectManager = PostEffectManager::GetInstance();
	postEffectManager->LoadPreset("TestGame");

	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initialize();
	YoRigine::JsonManager::SetCurrentScene("TitleScene");
	YoRigine::CollisionManager::GetInstance()->Initialize();
	YoRigine::ModelManipulator::GetInstance()->LoadScene("TitleScene");
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());

	YParticleManager::GetInstance().SetCamera(sceneCamera_.get());
	//------------------------------------------------------------
	// タイトル専用要素の初期化
	//------------------------------------------------------------
	titleUI_ = std::make_unique<TitleUI>();
	titleUI_->Initialize();

	player_ = std::make_unique<DemoPlayer>();
	player_->Initialize(sceneCamera_.get());
	player_->SetMotion("Idle1");

	auto playerCam = std::dynamic_pointer_cast<TitleCamera>(titleCamera);
	playerCam->SetTarget(player_->GetWT());
	playerCam->enableOrbit_ = true; // カメラ回転有効

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(sceneCamera_.get(), "Resources/DDS/vz_sinister_land_cubemap_ue.dds");

	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_.get());

	//------------------------------------------------------------
	// エディター登録（デバッグ用）
	//------------------------------------------------------------
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラエディター", [this]() {cameraEditor_->Update(); }, "Title");
	Editor::GetInstance()->RegisterGameUI("カメラモード切り替え", [this]() {UpdateCameraMode(); },"Title");
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

	auto* enemyHitEmitterGroup_ = YEmitterGroupManager::GetInstance().GetGroup("Title");
	enemyHitEmitterGroup_->EmitAll();

	YoRigine::ModelManipulator::GetInstance()->Update();
	YoRigine::CollisionManager::GetInstance()->Update();
	YParticleManager::GetInstance().Update(YoRigine::GameTime::GetDeltaTime());
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
	YParticleManager::GetInstance().Draw();


}

/// <summary>
/// オフスクリーン外の描画処理（現状未使用）
/// </summary>
void TitleScene::DrawNonOffscreen() {
	//------------------------------------------------------------
	// 2D UI描画
	//------------------------------------------------------------
	SpriteCommon::GetInstance()->DrawPreference();
	titleUI_->Draw();
}

/// <summary>
/// 影の描画
/// </summary>
void TitleScene::DrawShadow()
{
	DrawCommonShadow();
	player_->DrawShadow();
	YoRigine::ModelManipulator::GetInstance()->DrawShadow();
}

/// <summary>
/// 解放処理
/// </summary>
void TitleScene::Finalize() {
	YoRigine::JsonManager::ClearSceneInstances("TitleScene");
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
	if (ImGui::Button("Title Camera")) {
		cameraMode_ = CameraMode::CLEAR;
		CameraDirector::GetInstance()->SetEnableBlending(false);
	}
	if (ImGui::Button("Debug Camera")) {
		cameraMode_ = CameraMode::DEBUG;
		CameraDirector::GetInstance()->SetEnableBlending(false);
	}
#endif
}

void TitleScene::UpdateCamera() {
	auto director = CameraDirector::GetInstance();


	if (YoRigine::GameTime::IsPause()) {
		return;
	}

	// カメラの優先度
	switch (cameraMode_)
	{
	case CameraMode::TITLE:
		director->SetPriority("Title", 10);
		director->SetPriority("MainDebug", 0);
		director->SnapToActiveCamera();
		break;
	case CameraMode::DEBUG:
		director->SetPriority("MainDebug", 10);
		director->SetPriority("Title", 0);
		break;
	}


	//------------------------------------------------------------
	// デバッグ用：カメラ切り替え入力
	//------------------------------------------------------------
# ifdef _DEBUG	
	YoRigine::Input* input = YoRigine::Input::GetInstance();
	if (input->TriggerKey(DIK_F3)) {
		isDebugCamera_ = !isDebugCamera_;
		if (isDebugCamera_) {
			cameraMode_ = CameraMode::DEBUG;
			CameraDirector::GetInstance()->SetEnableBlending(false);
		}
		else {
			cameraMode_ = CameraMode::CLEAR;
			CameraDirector::GetInstance()->SetEnableBlending(false);
		}
	}
#endif

	//------------------------------------------------------------
	// Directorの更新（VirtualCameraの計算 ＋ ブレンド処理）
	//------------------------------------------------------------
	director->Update(YoRigine::GameTime::GetDeltaTime());

	//------------------------------------------------------------
	// 出力用カメラ(sceneCamera_)への同期
	//------------------------------------------------------------
	// Directorが導き出した「理想の座標・回転・レンズ情報」をコピー
	sceneCamera_->SetTranslate(director->GetActiveCameraPos());
	sceneCamera_->SetRotate(director->GetActiveCameraRot());
	sceneCamera_->SetFovY(director->GetFovY());
	sceneCamera_->viewMatrix_ = director->GetViewMatrix();
	// カメラ自体の更新（内部でのシェイク計算など）
	sceneCamera_->Update();
	// 最終的な行列の計算
	sceneCamera_->UpdateMatrix();
}