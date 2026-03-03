#include "MyGame.h"
#include "Particle./ParticleManager.h"
#include "Particle/ParticleEditor.h"
#include "Mesh/MeshPrimitive.h"
#include "Editor/Editor.h"
#include "Systems/GameTime/GameTime.h"
#include <ModelManipulator/ModelManipulator.h>
#include "OffScreen/PostEffectManager.h"
#include <Systems/UI/UIManager.h>
#include "GPUParticle/GpuEmitManager.h"

#include "Particle/YParticleManager.h"
#include "Particle/YEmitterGroupEditor.h"

#include "Particle/YEmitterGroupManager.h"
/// <summary>
/// ゲーム全体の初期化処理（起動時に一度だけ実行）
/// </summary>
void MyGame::Initialize() {

	//------------------------------------------------------------
	// 基盤・シーン管理の初期化
	//------------------------------------------------------------
	Framework::Initialize();
	texturePreloader_.PreloadDirectory("Resources/Textures/");
	audio_->PreloadAllInPath("Resources/Audio/");

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->SetTransitionFactory(std::make_unique<FadeTransitionFactory>());
	SceneManager::GetInstance()->Initialize();

	//------------------------------------------------------------
	// オフスクリーン / ポストエフェクト初期化
	//------------------------------------------------------------
	offScreen_ = OffScreen::GetInstance();
	offScreen_->Initialize();

	PostEffectManager::GetInstance()->Initialize();

	YParticleManager::GetInstance().Initialize(dxCommon_->GetSrvManager(), 50000);

	//YParticleManager::GetInstance().LoadSystemsFromFile("Resources/Json/YParticleSystems/EnemyHit1.json");
	//YParticleManager::GetInstance().LoadSystemsFromFile("Resources/Json/YParticleSystems/EnemyHit2.json");	
	//YEmitterGroupManager::GetInstance().LoadAllFromFile("Resources/Json/YEmitterGroups/EnemyHit.json");
	//------------------------------------------------------------
	// パーティクル関連の初期化
	//------------------------------------------------------------
	ParticleEditor::GetInstance().Initialize();
	YoRigine::ParticleManager::GetInstance()->Initialize(dxCommon_->GetSrvManager());
	YoRigine::GpuEmitManager::GetInstance()->Initialize();


	// パーティクルグループ作成
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("PlayerParticle", "Resources/images/circle.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("TestParticle", "Resources/images/circle.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("PlayerHitParticle", "Resources/images/gradationLine.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("GuardParticle", "Resources/images/gradationLine.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("ParryParticle", "Resources/Effects/star.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("TitleParticle", "Resources/Effects/star.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("ClearParticle", "Resources/Effects/starSmall.png");
	YoRigine::ParticleManager::GetInstance()->CreateParticleGroup("AreaParticle", "Resources/images/gradationLine.png");

	// メッシュ設定
	auto planeMesh = MeshPrimitive::CreatePlane(1.0f, 1.0f);
	auto cylinderMesh = MeshPrimitive::CreateCylinder(1.0f, 0.0f, 32, 1.0f);
	auto ringMesh = MeshPrimitive::CreateRing(1.0f, 0.5f, 256);

	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("PlayerParticle", planeMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("TestParticle", planeMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("PlayerHitParticle", ringMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("GuardParticle", ringMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("AreaParticle", ringMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("ParryParticle", planeMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("TitleParticle", planeMesh);
	YoRigine::ParticleManager::GetInstance()->SetPrimitiveMesh("ClearParticle", planeMesh);

	// 保存済みパーティクルの読み込み
	ParticleEditor::GetInstance().LoadAllSystems();

	// モデル操作関連の初期化
	YoRigine::ModelManipulator::GetInstance()->Initialize();

#ifdef USE_IMGUI
	//------------------------------------------------------------
	// エディター初期化とUI登録
	//------------------------------------------------------------
	Editor::GetInstance()->Initialize();

	// シーン変更コールバック登録
	Editor::GetInstance()->SetSceneChangeCallback(
		[](const std::string& sceneName) {
			if(sceneName == "Develop") SceneManager::GetInstance()->ChangeSceneImmediate(sceneName);
			else SceneManager::GetInstance()->ChangeScene(sceneName);
		}
	);

	Editor::GetInstance()->SetGizmoDrawCallback([]() {
		YoRigine::ModelManipulator::GetInstance()->DrawGizmo();
		YEmitterGroupEditor::GetInstance().DrawGizmo();
		});

	// 各種ImGuiツール登録
	Editor::GetInstance()->RegisterGameUI("ゲーム時間管理", &YoRigine::GameTime::ImGui);
	Editor::GetInstance()->RegisterGameUI("パーティクルエディター", []() { ParticleEditor::GetInstance().ShowEditor(); });
	Editor::GetInstance()->RegisterGameUI("モデル操作", []() { YoRigine::ModelManipulator::GetInstance()->DrawImGui(); });
	Editor::GetInstance()->RegisterGameUI("ポストエフェクト", []() { PostEffectManager::GetInstance()->ImGui(); });
	Editor::GetInstance()->RegisterGameUI("JSON管理", &YoRigine::JsonManager::ImGuiManager);
	Editor::GetInstance()->RegisterGameUI("UI管理", []() { YoRigine::UIManager::GetInstance()->ImGuiDebug(); });
	Editor::GetInstance()->RegisterGameUI("ログ", []() { Editor::GetInstance()->DrawLog(); });
	Editor::GetInstance()->RegisterGameUI("オーディオ詳細", [this]() { audio_->ShowDebugWindow(); });
	Editor::GetInstance()->RegisterGameUI("オーディオ設定", [this]() { audio_->ShowSettingsWindow(); });
#endif

	//------------------------------------------------------------
	// 初期シーン設定
	//------------------------------------------------------------
#ifdef _DEBUG
	SceneManager::GetInstance()->ChangeScene("Develop");   // デバッグ時はゲームシーン
#else
	SceneManager::GetInstance()->ChangeScene("Title");  // 製品版はタイトルシーン
#endif
}

/// <summary>
/// ゲーム終了時の解放処理
/// </summary>
void MyGame::Finalize() {
	SceneManager::GetInstance()->Finalize();
	YoRigine::ParticleManager::GetInstance()->Finalize();
	YoRigine::ModelManipulator::GetInstance()->Finalize();

#ifdef USE_IMGUI
	Editor::GetInstance()->Finalize();
#endif

	Framework::Finalize();
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
void MyGame::Update() {

	//------------------------------------------------------------
	// ImGui受付開始
	//------------------------------------------------------------
	imguiManager_->Begin();
#ifdef USE_IMGUI
	ImGuizmo::BeginFrame();
#endif
	texturePreloader_.FlushPendingUploads(10);

#ifdef USE_IMGUI
	Editor::GetInstance()->Draw();
#endif

	//------------------------------------------------------------
	// ゲーム本体の更新処理
	//------------------------------------------------------------
	Framework::Update();
	SceneManager::GetInstance()->Update();

	//------------------------------------------------------------
	// ImGui受付終了
	//------------------------------------------------------------
	imguiManager_->End();
}

/// <summary>
/// 描画処理
/// </summary>
void MyGame::Draw() {


	YoRigine::ModelManipulator::GetInstance()->DrawPickPass();

	//------------------------------------------------------------
	// オフスクリーン描画
	//------------------------------------------------------------
	dxCommon_->PreDrawShadow();
	SceneManager::GetInstance()->DrawShadow();
	dxCommon_->PreDrawOffScreen();
	dxCommon_->GetSrvManager()->PreDraw();

	// シーン描画
	SceneManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// ポストエフェクト描画
	//------------------------------------------------------------
	dxCommon_->PreDraw();
	offScreen_->SetProjection(SceneManager::GetInstance()->GetScene()->GetViewProjection());
	PostEffectManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// 通常描画 + UI
	//------------------------------------------------------------
	dxCommon_->DepthBarrier();
	SceneManager::GetInstance()->DrawNonOffscreen();
	dxCommon_->CopyBackBufferToFinalResult();
	imguiManager_->Draw();

	// フレーム終了
	dxCommon_->PostDraw();
}