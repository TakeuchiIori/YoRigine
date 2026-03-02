#include "ClearScene.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Systems./Input./Input.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Sprite/SpriteCommon.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include "Loaders/Json/JsonManager.h"
#include "ModelManipulator/ModelManipulator.h"
#include "OffScreen/PostEffectManager.h"
#include <Debugger/Logger.h>
#include "Particle/YParticleManager.h"
#include "Particle/YParticleEditor.h"

// Camera
#include "Systems/Camera/Virtuals/DebugCamera/DebugCamera.h"
#include "Systems/Camera/Virtuals/ClearCamera/ClearCamera.h"

/// <summary>
/// 初期化処理
/// </summary>
void ClearScene::Initialize() {


	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------

	// 出力用カメラの実体を生成
	sceneCamera_ = std::make_unique<Camera>();
	auto director = CameraDirector::GetInstance();
	director->Initialize();


	cameraEditor_ = std::make_unique<CameraEditor>();
	cameraEditor_->Initialize();
	cameraEditor_->SetFilePath("Resources/Json/VirtualCameraData/ClearScene.json");
	cameraEditor_->LoadFileOrDefault(cameraEditor_->GetFilePath(), "Clear");
	cameraMode_ = CameraMode::CLEAR;
	// カメラの登録
	auto clearCamera = director->GetCamera("Clear");
	auto debug = director->GetCamera("MainDebug");



	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initialize();
	YoRigine::JsonManager::SetCurrentScene("ClearScene");
	YoRigine::CollisionManager::GetInstance()->Initialize();
	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->LoadScene("ClearScene");
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());
	YParticleManager::GetInstance().SetCamera(sceneCamera_.get());
	

	//------------------------------------------------------------
	// クリア画面スプライトの生成と設定
	//------------------------------------------------------------
	clearUI_ = std::make_unique<ClearUI>();
	clearUI_->Initialize();

	//------------------------------------------------------------
	// オブジェクトの生成
	//------------------------------------------------------------

	player_ = std::make_unique<DemoPlayer>();
	player_->Initialize(sceneCamera_.get());
	player_->SetMotion("Idle2");

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(sceneCamera_.get(), "Resources/DDS/vz_sinister_land_cubemap_ue.dds");

	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_.get());

#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラエディター", [this]() {cameraEditor_->Update(); }, "Clear");
	Editor::GetInstance()->RegisterGameUI("カメラモード切り替え", [this]() {UpdateCameraMode(); }, "Clear");
	Editor::GetInstance()->RegisterGameUI("ライティング", [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); }, "Clear");
	Editor::GetInstance()->RegisterGameUI("YoRigine:パーティクルエディター", [this]() {YParticleEditor::GetInstance().ShowEditorWindow(); }, "Clear");
#endif
}

/// <summary>
/// 終了処理
/// </summary>
void ClearScene::Finalize() {
	YoRigine::JsonManager::ClearSceneInstances("ClearScene");
}

/// <summary>
/// 更新処理
/// </summary>
void ClearScene::Update() {
	YoRigine::GameTime::Update();
	if (YoRigine::Input::GetInstance()->IsPadPressed(0, GamePadButton::A)) {
		SceneManager::GetInstance()->ChangeScene("Title");
	}
	UpdateCamera();
	clearUI_->Update();

	player_->Update();
	skyBox_->Update();
	ground_->Update();



	YoRigine::ParticleManager::GetInstance()->Emit("ClearParticle", Vector3(0, 0, 0), 10);
	YoRigine::ModelManipulator::GetInstance()->Update();
	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
	YParticleManager::GetInstance().Update(YoRigine::GameTime::GetDeltaTime());
}

/// <summary>
/// 描画処理
/// </summary>
void ClearScene::Draw() {

	//------------------------------------------------------------
	// 3D描画（背景・キャラ）
	//------------------------------------------------------------
	skyBox_->Draw();
	Object3dCommon::GetInstance()->DrawPreference();
	DrawObject();

	//------------------------------------------------------------
	// 演出関連の描画（パーティクルなど）
	//------------------------------------------------------------
	YoRigine::ParticleManager::GetInstance()->Draw();

	YParticleManager::GetInstance().Draw();

	//------------------------------------------------------------
	// 2Dスプライト描画
	//------------------------------------------------------------
	SpriteCommon::GetInstance()->DrawPreference();
	clearUI_->DrawAll();

	//------------------------------------------------------------
	// 3Dオブジェクト描画（必要時に追加）
	//------------------------------------------------------------
	Object3dCommon::GetInstance()->DrawPreference();
}

/// <summary>
/// オフスクリーン外の描画処理
/// </summary>
void ClearScene::DrawNonOffscreen() {
	// 今のところ特に処理なし（タイトル演出など追加予定）
}

/// <summary>
/// 影の描画
/// </summary>
void ClearScene::DrawShadow()
{
	YoRigine::ModelManipulator::GetInstance()->DrawShadow();
}

/// <summary>
/// オブジェクト描画（地面・プレイヤー）
/// </summary>
void ClearScene::DrawObject() {
	ground_->Draw();
	player_->Draw();
	player_->DrawAnimation();
	YoRigine::ModelManipulator::GetInstance()->Draw();
}

/// <summary>
/// ライン描画（デバッグ用）
/// </summary>
void ClearScene::DrawLine() {}

/// <summary>
/// UI描画（必要に応じて拡張）
/// </summary>
void ClearScene::DrawUI() {}

/// <summary>
/// カメラモード切り替えUI（ImGui）
/// </summary>
void ClearScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	if (ImGui::Button("Clear Camera")) {
		cameraMode_ = CameraMode::CLEAR;
		CameraDirector::GetInstance()->SetEnableBlending(false);
	}
	if (ImGui::Button("Debug Camera")) {
		cameraMode_ = CameraMode::DEBUG;
		CameraDirector::GetInstance()->SetEnableBlending(false);
	}
#endif
}

void ClearScene::UpdateCamera() {
	auto director = CameraDirector::GetInstance();


	if (YoRigine::GameTime::IsPause()) {
		return;
	}

	// カメラの優先度
	switch (cameraMode_)
	{
	case CameraMode::CLEAR:
		director->SetPriority("Clear", 10);
		director->SetPriority("MainDebug", 0);
		director->SnapToActiveCamera();
		break;
	case CameraMode::DEBUG:
		director->SetPriority("MainDebug", 10);
		director->SetPriority("Clear", 0);
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