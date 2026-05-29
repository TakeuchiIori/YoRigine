#include "DevelopScene.h"

// Engine
#include "Systems./Input./Input.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include "Loaders/Json/JsonManager.h"
#include "ModelManipulator/ModelManipulator.h"
#include "OffScreen/PostEffectManager.h"
#include <Debugger/Logger.h>
#include <SceneSystems/SceneManager.h>
#include "Collision/Core/CollisionManager.h"

#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/AreaCollision/Base/AreaManager.h"

#include "Particle./ParticleManager.h"
#include "Particle/YParticleManager.h"
#include "Particle/YParticleEditor.h"
#include "Particle/YEmitterGroupEditor.h"
#include "GPUParticle/GpuEmitManager.h"
#include <Vfx/VfxMesh/VfxMeshEditor.h>


// Camera
#include "Systems/Camera/Virtuals/DebugCamera/DebugCamera.h"

// C++
#include <cstdlib>
#include <ctime>
#include <Collision/AreaCollision/CircleArea.h>
#include <Collision/AreaCollision/Base/AreaEditor.h>

// ============================================================
// シーンの初期化
// ============================================================
void DevelopScene::Initialize() {

	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------

	// 出力用カメラの実体を生成
	sceneCamera_ = std::make_unique<Camera>();
	auto director = CameraDirector::GetInstance();
	director->Initialize();

	// カメラエディタの登録
	cameraEditor_ = std::make_unique<CameraEditor>();
	cameraEditor_->Initialize();
	cameraEditor_->SetFilePath("Resources/Json/VirtualCameraData/DevelopScene.json");
	cameraEditor_->LoadFileOrDefault(cameraEditor_->GetFilePath(), "Develop");
	
	// デバッグモード
	cameraMode_ = CameraMode::DEBUG;


	// ライン
	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(sceneCamera_.get());

	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initialize();
	YoRigine::JsonManager::SetCurrentScene("DevelopScene");
	YParticleManager::GetInstance().SetCamera(sceneCamera_.get());
	AreaManager::GetInstance()->Initialize();
	YoRigine::CollisionManager::GetInstance()->Initialize();
#ifdef USE_IMGUI
	YEmitterGroupEditor::GetInstance().SetCamera(sceneCamera_.get());
	YoRigine::VfxMeshEditor::GetInstance()->Initialize();
	YoRigine::VfxMeshEditor::GetInstance()->SetCamera(sceneCamera_.get());
#endif
	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->LoadScene("DevelopScene");

	YoRigine::GpuEmitManager::GetInstance()->SetCamera(sceneCamera_.get());


	// エリア設定
	auto battleFieldArea = std::make_shared<CircleArea>();
	battleFieldArea->Initialize(Vector3(0, 0, 0), 50.0f);
	battleFieldArea->SetPurpose(AreaPurpose::Boundary);  // 明示
	battleFieldArea->SetCamera(sceneCamera_.get());

	auto* mgr = AreaManager::GetInstance();
	mgr->AddArea("FieldArea", battleFieldArea);
	mgr->SetDebugDrawEnabled(true);

	//------------------------------------------------------------
	// エディター用GUI登録
	//------------------------------------------------------------
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラエディター", [this]() {cameraEditor_->Update(); }, "Develop");
	Editor::GetInstance()->RegisterGameUI("ライティング", [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); }, "Develop");
	Editor::GetInstance()->RegisterGameUI("GpuParticle", [this]() { YoRigine::GpuEmitManager::GetInstance()->DrawImGui(); }, "Develop");
	Editor::GetInstance()->RegisterGameUI("YoRigine:パーティクルエディター", [this]() {YParticleEditor::GetInstance().ShowEditorWindow(); }, "Develop");
	//Editor::GetInstance()->RegisterGameUI("モーションエディタ", [this]() {motionEditor_->ShowEditor(); }, "Develop");
	Editor::GetInstance()->RegisterGameUI("VFX", [this]() { YoRigine::VfxMeshEditor::GetInstance()->DrawImGui(); }, "Develop");

#endif
}

// ============================================================
// シーンの更新
// ============================================================
void DevelopScene::Update() {
	YoRigine::GameTime::Update();
	UpdateCamera();


	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::ModelManipulator::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
	YParticleManager::GetInstance().Update(YoRigine::GameTime::GetDeltaTime());
	YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(sceneCamera_.get());
	YoRigine::GpuEmitManager::GetInstance()->Update();

#ifdef USE_IMGUI
	AreaEditor::GetInstance()->Update();
	YoRigine::VfxMeshEditor::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
#endif
}

// ============================================================
// シーンの描画（PostEffectがかかる）
// ============================================================
void DevelopScene::Draw() {
	//------------------------------------------------------------
	// 3Dオブジェクト描画
	//------------------------------------------------------------
	Object3dCommon::GetInstance()->DrawPreference();
	DrawObject();

	//------------------------------------------------------------
	// パーティクル描画
	//------------------------------------------------------------
	YoRigine::ParticleManager::GetInstance()->Draw();
	YParticleManager::GetInstance().Draw();
	DrawLine();
	YoRigine::GpuEmitManager::GetInstance()->Draw();

#ifdef USE_IMGUI
	YoRigine::VfxMeshEditor::GetInstance()->DrawPreview();
#endif
	
}

// ============================================================
// PostEffectを掛けたくないものを描画
// ============================================================
void DevelopScene::DrawNonOffscreen() {
}

// ============================================================
// 影の描画
// ============================================================
void DevelopScene::DrawShadow() {
	DrawCommonShadow();
	YoRigine::ModelManipulator::GetInstance()->DrawShadow();
}

// ============================================================
// 終了の処理
// ============================================================
void DevelopScene::Finalize() {
}

// ============================================================
// オブジェクトの描画
// ============================================================
void DevelopScene::DrawObject() {
	YoRigine::ModelManipulator::GetInstance()->Draw();
}

// ============================================================
// 線の描画
// ============================================================
void DevelopScene::DrawLine() {
	YoRigine::ModelManipulator::GetInstance()->DrawLine();
	AreaManager::GetInstance()->DrawArea("FieldArea", line_.get());
	line_->DrawLine();
}


// ============================================================
// カメラの更新処理
// ============================================================
void DevelopScene::UpdateCamera() {
	auto director = CameraDirector::GetInstance();

	if (YoRigine::GameTime::IsPause()) {
		return;
	}

	// カメラの優先度
	switch (cameraMode_) {
	case CameraMode::DEBUG:
		director->SetPriority("MainDebug", 10);
		break;
	}

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

