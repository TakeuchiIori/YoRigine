#include "GameScene.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Sprite/SpriteCommon.h"
#include <Editor/Editor.h>
#include "Loaders/Json/JsonManager.h"
#include "ModelManipulator/ModelManipulator.h"
#include "OffScreen/PostEffectManager.h"
#include <Debugger/Logger.h>
#include "Systems./Input./Input.h"
#include "Systems/GameTime/GameTime.h"
#include <Systems/UI/UIManager.h>
#include "GPUParticle/GpuEmitManager.h"
#include "Collision/AreaCollision/Base/AreaManager.h"
#include <Systems/Audio/Audio.h>

#include "Particle/YParticleManager.h"
#include "Particle/YParticleEditor.h"
// Camera
#include "Systems/Camera/CameraFactory.h"
#include "Systems/Camera/CameraEditor.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"
#include "Systems/Camera/Virtuals/DebugCamera/DebugCamera.h"
#include <Vfx/VfxMesh/VfxMeshEditor.h>

// C++
#include <cstdlib>
#include <ctime>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// 初期化処理
/// </summary>
void GameScene::Initialize() {
	srand(static_cast<unsigned int>(time(nullptr)));


	//------------------------------------------------------------
	// カメラ初期化
	//------------------------------------------------------------

	// 出力用カメラの実体を生成
	sceneCamera_ = std::make_unique<Camera>();
	auto director = CameraDirector::GetInstance();
	director->Initialize();


	cameraEditor_ = std::make_unique<CameraEditor>();
	cameraEditor_->Initialize();
	cameraEditor_->SetFilePath("Resources/Json/VirtualCameraData/GameScene.json");
	cameraEditor_->LoadFileOrDefault(cameraEditor_->GetFilePath(), "Game");


	cameraMode_ = CameraMode::FOLLOW;
	//------------------------------------------------------------
	// システム初期化
	//------------------------------------------------------------
	YoRigine::GameTime::Initialize();
	YoRigine::JsonManager::SetCurrentScene("GameScene");
	YoRigine::CollisionManager::GetInstance()->Initialize();
	YoRigine::ModelManipulator::GetInstance()->LoadScene("GameScene");
	AreaManager::GetInstance()->Initialize();
	YParticleManager::GetInstance().SetCamera(sceneCamera_.get());

	// エミッターを作成（システム名と初期位置を指定）
	explosionEmitter_ = std::make_unique<YParticleEmitter>(
		"DefaultParticleSystem",        // 使用するシステム名
		Vector3{ 0, 0, 0 }    // 初期位置
	);

	// エミッター設定
	explosionEmitter_->SetEmissionRate(50.0f);  // 秒間50個発生
	explosionEmitter_->SetEmitCount(5);         // 1回に5個発生
	//------------------------------------------------------------
	// インゲーム用UI
	//------------------------------------------------------------
	gameUI_ = std::make_unique<GameUI>();
	gameUI_->Initialize();


	//------------------------------------------------------------
	// 共通システム初期化
	//------------------------------------------------------------

	// AttackData の JSON を読み込む
	AttackDatabase::LoadFromFile("Resources/Json/Combo/AttackData.json");
#ifdef USE_IMGUI
	// エディターを初期化
	attackEditor_ = std::make_unique<AttackDataEditor>();
	attackEditor_->SetFilePath("Resources/Json/Combo/AttackData.json");
	attackEditor_->SetAutoReload(true);  // 自動リロード有効
	attackEditor_->SetOpen(true);

	// リロードコールバックを設定
	attackEditor_->SetReloadCallback([this]() {
		// プレイヤーのコンボシステムをリロード
		if (player_) {
			player_->GetCombat()->GetCombo()->ReloadAttacks();
		}
		Logger("[GameScene] Attack data reloaded from editor!\n");
		});
#endif

	//------------------------------------------------------------
	// 共通オブジェクト
	//------------------------------------------------------------



	player_ = std::make_unique<Player>();
	player_->Initialize(sceneCamera_.get()); 
	auto playerCam = std::dynamic_pointer_cast<FollowCamera>(director->GetCamera("PlayerFollow"));
	director->RegisterTarget("Player", &player_->GetWT());

	if (playerCam) {
		playerCam->SetTarget(&player_->GetWT(), "Player");
	}
	// フォローカメラを取得（JSON読み込み後に存在する）
	auto follow = director->GetCamera("PlayerFollow");
	auto followPtr = std::static_pointer_cast<FollowCamera>(follow);


	// デバッグカメラを取得または作成
	auto debug = director->GetCamera("MainDebug");
	director->SnapToActiveCamera();

	// プレイヤーにフォローカメラを設定
	if (followPtr) {
		player_->SetFollowCamera(followPtr.get());
	}
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(sceneCamera_.get(), "Resources/DDS/vz_classic_cubemap_ue.dds");


	YoRigine::ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::ModelManipulator::GetInstance()->SetCamera(sceneCamera_.get());
	YoRigine::GpuEmitManager::GetInstance()->SetCamera(sceneCamera_.get());

	//------------------------------------------------------------
	// サブシーン管理初期化
	//------------------------------------------------------------
	subSceneManager_ = std::make_unique<SubSceneManager>();
	subSceneManager_->Initialize(sceneCamera_.get(), player_.get());

	// フィールドシーン登録
	auto fieldScene = std::make_unique<FieldScene>();
	fieldScene->Initialize(sceneCamera_.get(), player_.get());
	subSceneManager_->RegisterSubScene("Field", std::move(fieldScene));

	// バトルシーン登録
	auto battleScene = std::make_unique<BattleScene>();
	battleScene->Initialize(sceneCamera_.get(), player_.get());

	//------------------------------------------------------------
	// バトル終了後にフィールドへ戻すコールバック
	//------------------------------------------------------------
	battleScene->SetBattleEndCallback([this](FieldReturnData fieldData, BattleResult result, const BattleStats& stats) {
		FieldReturnData returnData;
		returnData.playerWon = (result == BattleResult::Victory);
		returnData.expGained = stats.totalExpGained;
		returnData.goldGained = stats.totalGaldGained;
		returnData.itemsGained = stats.droppedItems;
		returnData.defeatedEnemyGroup = fieldData.defeatedEnemyGroup;

		// フィールド復帰処理
		auto* field = dynamic_cast<FieldScene*>(subSceneManager_->GetScene("Field"));
		if (field) {
			SubSceneTransitionRequest request;
			request.type = SubSceneTransitionType::TO_FIELD;
			request.transitionData = new FieldReturnData(returnData);

			subSceneManager_->HandleTransitionRequest(request);
			subSceneManager_->SwitchToSceneWithFade("Field");
			field->HandleBattleReturn(returnData);
		}
		});

	subSceneManager_->RegisterSubScene("Battle", std::move(battleScene));

	// 初期サブシーンをフィールドに設定
	subSceneManager_->SwitchToScene("Field");

	//------------------------------------------------------------
	// エディター用GUI登録
	//------------------------------------------------------------
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("カメラエディター", [this]() {cameraEditor_->Update(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("カメラモード切り替え", [this]() {UpdateCameraMode(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("プレイヤーの状態情報", [this]() {player_->DrawImGui(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("ライティング", [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("GpuParticle", [this]() { YoRigine::GpuEmitManager::GetInstance()->DrawImGui(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("プレイヤー攻撃エディター", [this]() {attackEditor_->DrawImGui(); }, "Game");
	Editor::GetInstance()->RegisterGameUI("YoRigine:パーティクルエディター", [this]() {YParticleEditor::GetInstance().ShowEditorWindow(); }, "Game");
#endif

	//------------------------------------------------------------
	// BGM再生
	//------------------------------------------------------------

	//static auto bgm = YoRigine::Audio::GetInstance()->Play(
	//	"Resources/Audio/BGM2.mp3",
	//	true,
	//	0.2f,
	//	YoRigine::SoundCategory::BGM
	//);
}

/// <summary>
/// 更新処理
/// </summary>
void GameScene::Update() {
	YoRigine::GameTime::Update();
	Player* player = player_.get();

	// エミッター位置を更新
	explosionEmitter_->SetPosition(currentPosition_);

	// エミッター更新（autoEmitがtrueなら自動発生）
	explosionEmitter_->Update(YoRigine::GameTime::GetDeltaTime());

#ifdef USE_IMGUI
	//explosionEmitter_->ShowDebugInfo();
#endif

	UpdateCamera();
	bool isPlayerDead = player->GetCombat()->IsDead();
	auto director = CameraDirector::GetInstance();
	auto follow = director->GetCamera("PlayerFollow");
	auto followPtr = std::static_pointer_cast<FollowCamera>(follow);

	//------------------------------------------------------------
	// ゲームオーバー時の選択処理
	//------------------------------------------------------------
	if (isPlayerDead != wasPlayerDead_) {
		if (isPlayerDead) {
			// 死亡した瞬間:フェード開始
			gameUI_->ShowGameOverWithFade(3.0f);
		} else {
			// 復活した瞬間:リセット
			gameUI_->ResetGameOver();
		}
		wasPlayerDead_ = isPlayerDead;
	}
	followPtr->SetIsCloseUp(isPlayerDead);
	// リトライ処理
	if (gameUI_->IsRetryRequested()) {
		HandleRetry();
		gameUI_->ClearRequests();
	}
	// タイトルへ戻る処理（ポーズ画面 or ゲームオーバー画面）
	else if (gameUI_->IsReturnToTitleRequested()) {
		HandleReturnToTitle();
		gameUI_->ClearRequests();
	}


	// カメラモード同期
	if (subSceneManager_) {
		cameraMode_ = subSceneManager_->GetCameraMode();
	}

	// サブシーン更新
	if (subSceneManager_) {
		subSceneManager_->Update();
	}

	// 空と共通システム更新
	skyBox_->Update();
	gameUI_->Update();


	YoRigine::ModelManipulator::GetInstance()->Update();
	YoRigine::CollisionManager::GetInstance()->Update();
	YoRigine::ParticleManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());
	YParticleManager::GetInstance().Update(YoRigine::GameTime::GetDeltaTime());
	YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(sceneCamera_.get());
	YoRigine::LightManager::GetInstance()->TransferData();
	YoRigine::GpuEmitManager::GetInstance()->Update();
}

/// <summary>
/// 描画処理
/// </summary>
void GameScene::Draw() {
	//------------------------------------------------------------
	// 3Dオブジェクト描画
	//------------------------------------------------------------
	skyBox_->Draw();
	Object3dCommon::GetInstance()->DrawPreference();
	DrawObject();
	YoRigine::ModelManipulator::GetInstance()->Draw();

	//------------------------------------------------------------
	// パーティクル描画
	//------------------------------------------------------------
	YoRigine::ParticleManager::GetInstance()->Draw();
	YParticleManager::GetInstance().Draw();
	DrawLine();
	YoRigine::GpuEmitManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// VFX描画
	//------------------------------------------------------------
	player_->DrawVfx();

	//------------------------------------------------------------
	// UI描画
	//------------------------------------------------------------
	SpriteCommon::GetInstance()->DrawPreference();
	DrawUI();
}

/// <summary>
/// オフスクリーン対象外のUI描画
/// </summary>
void GameScene::DrawNonOffscreen() {
	SpriteCommon::GetInstance()->DrawPreference();
	if (subSceneManager_) {
		subSceneManager_->DrawNonOffscreen();
	}
}

/// <summary>
/// 影の描画
/// </summary>
void GameScene::DrawShadow()
{
	if (subSceneManager_) {
		subSceneManager_->DrawShadow();
	}
	YoRigine::ModelManipulator::GetInstance()->DrawShadow();
}

/// <summary>
/// サブシーンのオブジェクト描画
/// </summary>
void GameScene::DrawObject() {
	if (subSceneManager_) {
		subSceneManager_->DrawObject();
	}
}

/// <summary>
/// サブシーンのライン描画
/// </summary>
void GameScene::DrawLine() {
	if (subSceneManager_) {
		subSceneManager_->DrawLine();
	}
}

/// <summary>
/// UI描画（現在空）
/// </summary>
void GameScene::DrawUI() {
	//gameUI_->DrawAll();
	gameUI_->Draw();
	if (subSceneManager_) {
		subSceneManager_->DrawUI();
	}
}

/// <summary>
/// カメラ更新処理
/// </summary>
void GameScene::UpdateCamera() {
	auto director = CameraDirector::GetInstance();


	if (YoRigine::GameTime::IsPause()) {
		return;
	}

	// カメラの優先度
	switch (cameraMode_)
	{
	case CameraMode::FOLLOW:
		// フォローカメラ優先
		director->SetPriority("PlayerFollow", 10);
		director->SetPriority("MainDebug", 0);
		//director->SetPriority("BattleStartCamera", 0);
		break;
	case CameraMode::BATTLE_START:
		// バトル開始カメラ優先
		//director->SetPriority("BattleStartCamera", 10);
		director->SetPriority("PlayerFollow", 0);
		director->SetPriority("MainDebug", 0);
		break;
	case CameraMode::DEBUG:
		director->SetPriority("MainDebug", 10);
		director->SetPriority("PlayerFollow", 0);
		//director->SetPriority("BattleStartCamera", 0);
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
			// デバッグカメラを最優先に
			cameraMode_ = CameraMode::DEBUG;
			CameraDirector::GetInstance()->SetEnableBlending(false);
		} else {
			// 通常のフォローカメラを優先に
			cameraMode_ = CameraMode::FOLLOW;
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

/// <summary>
/// カメラモード切り替え（ImGui）
/// </summary>
void GameScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	if (ImGui::Button("Follow Camera")) {
		cameraMode_ = CameraMode::FOLLOW;
		CameraDirector::GetInstance()->SetEnableBlending(false);
	}
	if (ImGui::Button("Battle Start Camera")) cameraMode_ = CameraMode::BATTLE_START;
	if (ImGui::Button("Debug Camera")) {
		cameraMode_ = CameraMode::DEBUG;
		CameraDirector::GetInstance()->SetEnableBlending(false);
	}
	if (subSceneManager_) subSceneManager_->SetCameraMode(cameraMode_);
#endif
}

/// <summary>
/// リトライ処理
/// </summary>
void GameScene::HandleRetry() {
	Logger("[GameScene] Retry requested - Restarting Field Scene\n");

	// プレイヤーをリセット
	if (player_) {
		player_->Reset();
	}

	// ゲームオーバーUIをリセット
	gameUI_->ResetGameOver();
	wasPlayerDead_ = false;

	// フィールドシーンを再初期化
	if (subSceneManager_) {
		auto* fieldScene = dynamic_cast<FieldScene*>(subSceneManager_->GetScene("Field"));
		if (fieldScene) {
			// フィールドシーンを再初期化
			fieldScene->Initialize(sceneCamera_.get(), player_.get());
		}

		// フィールドシーンに切り替え
		subSceneManager_->SwitchToSceneWithFade("Field");
	}

	isDebugCamera_ = false;
	//auto director = CameraDirector::GetInstance();
	//director->SetPriority("MainDebug", 0);
	//director->SetPriority("PlayerFollow", 10);

	// ゲーム時間を再開
	YoRigine::GameTime::Resume();

	Logger("[GameScene] Field Scene restarted\n");
}

/// <summary>
/// タイトルへ戻る処理
/// </summary>
void GameScene::HandleReturnToTitle() {
	Logger("[GameScene] Return to Title requested\n");

	// タイトルシーンへ遷移
	SceneManager::GetInstance()->ChangeScene("Title");
	Logger("[GameScene] Changing to Title Scene\n");
}

/// <summary>
/// ゲームクリア処理
/// </summary>
void GameScene::HandleGameClear() {
	Logger("[GameScene] Game Clear! All enemies defeated!\n");

	// クリアシーンへ遷移
	SceneManager::GetInstance()->ChangeScene("Clear");
	Logger("[GameScene] Changing to Clear Scene\n");
}

/// <summary>
/// 終了処理
/// </summary>
void GameScene::Finalize() {
	YoRigine::JsonManager::ClearSceneInstances("GameScene");
	if (subSceneManager_) subSceneManager_->Finalize();
	subSceneManager_ = nullptr;
}