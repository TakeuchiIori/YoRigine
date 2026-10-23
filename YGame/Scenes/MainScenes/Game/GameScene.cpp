#include "GameScene.h"

// Engine
#include "Collision/AreaCollision/Base/AreaEditor.h"
#include "Collision/AreaCollision/Base/AreaManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Composite/CompositeEffectManager.h"
#include "GPUParticle/YGpuEmitManager.h"
#include "LightManager/LightManager.h"
#include "Loaders/Json/JsonManager.h"
#include "MathFunc.h"
#include "Object3D/Object3dCommon.h"
#include "OffScreen/PostEffectManager.h"
#include "SceneEditor/SceneEditor.h"
#include "Sprite/SpriteCommon.h"
#include "Systems./Input./Input.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Tutorial/TutorialSpotlight.h"
#include <Debugger/Logger.h>
#include <Editor/Editor.h>
#include <SceneSystems/SceneManager.h>
#include <Systems/Audio/Audio.h>
#include <Systems/UI/UIManager.h>

// App
#include "GameIds.h"
#include "SubScenes/BattleScene.h"
#include "SubScenes/FieldScene.h"

#include "Particle/YParticleEditor.h"
#include "Particle/YParticleManager.h"
// Camera
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"
#include <Vfx/VfxMesh/Editor/VfxMeshEditor.h>
#include <Vfx/VfxMesh/Runtime/VfxMeshSpawner.h>

// C++
#include <cstdlib>
#include <ctime>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

GameScene::GameScene() : BaseScene(SceneId::Game) {}

/// <summary>
/// 初期化処理
/// </summary>
void GameScene::Initialize() {
  srand(static_cast<unsigned int>(time(nullptr)));

  //------------------------------------------------------------
  // カメラ初期化
  //------------------------------------------------------------

  SceneCameraRig::Desc cameraDesc;
  cameraDesc.editorFilePath = "Resources/Json/VirtualCameraData/GameScene.json";
  cameraDesc.editorPresetName = "Game";
  cameraDesc.mainCameraId = CameraId::PlayerFollow;
  // デバッグカメラで見回している間はゲームを止める
  cameraDesc.pauseGameTimeInDebug = true;
  cameraRig_.Initialize(cameraDesc);
  YoRigine::Camera *sceneCamera = cameraRig_.GetCamera();
  auto director = CameraDirector::GetInstance();

  // ゲーム用のポストエフェクト（TestGame: Fog + GodRays 等）
  PostEffectManager::GetInstance()->LoadPreset("TestGame");

  //------------------------------------------------------------
  // システム初期化
  //------------------------------------------------------------
  YoRigine::GameTime::Initialize();
  YoRigine::JsonManager::SetCurrentScene("GameScene");
  YoRigine::CollisionManager::GetInstance()->Initialize();
  // 視錐台外コライダーは BroadPhase 登録をスキップする (個別オプトアウトは
  // BaseCollider::SetCheckOutsideCamera(false))
  YoRigine::CollisionManager::GetInstance()->SetCullingCamera(sceneCamera);
  YoRigine::CollisionManager::GetInstance()->SetEnableFrustumCulling(true);
  // SceneEditor のシーン読み込みは FieldScene / BattleScene の OnEnter で
  // 各サブシーン用の JSON ("Field.json" / "Battle.json") を読む方式に統一した。
  // ここで GameScene.json を読むと直後の SwitchToScene → OnEnter で上書きされる
  // だけなので、ロードはしない。
  AreaManager::GetInstance()->Initialize();
  YParticleManager::GetInstance().SetCamera(sceneCamera);

  //------------------------------------------------------------
  // インゲーム用UI
  //------------------------------------------------------------
  gameUI_ = std::make_unique<GameUI>();
  gameUI_->Initialize();

  //------------------------------------------------------------
  // 共通オブジェクト
  //------------------------------------------------------------

  player_ = std::make_unique<Player>();
  player_->Initialize(sceneCamera);
  director->RegisterTarget("Player", &player_->GetWT());

  // フォローカメラを取得（JSON読み込み後に存在する）
  auto followPtr = std::dynamic_pointer_cast<FollowCamera>(
      director->GetCamera(CameraId::PlayerFollow));
  if (followPtr) {
    followPtr->SetTarget(&player_->GetWT(), "Player");
  }

  // JSON保存値で MainDebug が高優先のまま残っていると、SnapToActiveCamera が
  // MainDebug にスナップ → 次フレームの UpdateCamera で PlayerFollow へ切替が
  // 発生し、巨大累積角度（例: 99236rad）から Euler 線形補間でグルグル回る。
  // 初期モードに合わせて優先度を確定してからスナップする。
  director->SetPriority(CameraId::ClearCinematic, 0);
  cameraRig_.SnapToActive();

  // PlayerCamera を初期化（FollowCamera を内包するコンポーネント構成）
  if (followPtr) {
    player_->InitializeCamera(followPtr.get());
  }
  skyBox_ = std::make_unique<SkyBox>();
  skyBox_->Initialize(sceneCamera, "Resources/DDS/vz_classic_cubemap_ue.dds");

  YoRigine::SceneEditor::GetInstance()->SetCamera(sceneCamera);
  YoRigine::YGpuEmitManager::GetInstance()->SetCamera(sceneCamera);

  //------------------------------------------------------------
  // チュートリアルのスポットライト
  //   ワールド上の対象へ穴を開けるには、投影用カメラと位置の取得先が要る。
  //   位置は関数で渡すので、対象が動いても矩形が追従する。
  //------------------------------------------------------------
  auto *spotlight = YoRigine::TutorialSpotlight::GetInstance();
  spotlight->SetCamera(sceneCamera);
  spotlight->RegisterWorldTarget(
      "Player", [this]() { return player_->GetWorldPosition(); });

  //------------------------------------------------------------
  // サブシーン管理初期化
  //------------------------------------------------------------
  subSceneManager_ = std::make_unique<SubSceneManager>();
  subSceneManager_->Initialize();
  // サブシーンはメインシーンの遷移を決めず、ここへ通知してくる
  subSceneManager_->SetEventHandler(
      [this](SubSceneEvent event) { HandleSubSceneEvent(event); });

  // フィールドシーン登録
  auto fieldScene = std::make_unique<FieldScene>();
  fieldScene->Initialize(sceneCamera, player_.get());
  subSceneManager_->RegisterSubScene(SubSceneId::Field, std::move(fieldScene));

  // バトルシーン登録
  // フィールドへの復帰は BattleScene::HandleBattleEnd 内で
  // RequestFieldTransition(returnData) を呼ぶので、ここでは何もしない。
  auto battleScene = std::make_unique<BattleScene>();
  battleScene->Initialize(sceneCamera, player_.get());
  subSceneManager_->RegisterSubScene(SubSceneId::Battle, std::move(battleScene));

  // 初期サブシーンをフィールドに設定
  subSceneManager_->SwitchToScene(SubSceneId::Field);

  //------------------------------------------------------------
  // エディター用GUI登録
  //------------------------------------------------------------
  AttackDatabase::LoadFromFile("Resources/Json/Combo/AttackData.json");
#ifdef USE_IMGUI
  // エディターを初期化
  attackEditor_ = std::make_unique<AttackDataEditor>();
  attackEditor_->SetFilePath("Resources/Json/Combo/AttackData.json");
  attackEditor_->SetPlayer(player_.get());
  attackEditor_->SetAutoReload(true); // 自動リロード有効
  attackEditor_->SetOpen(true);

  // リロードコールバックを設定
  attackEditor_->SetReloadCallback([this]() {
    // プレイヤーのコンボシステムをリロード
    if (player_) {
      player_->GetCombat()->GetCombo()->ReloadAttacks();
    }
    Logger("[GameScene] Attack data reloaded from editor!\n");
  });

  cameraRig_.RegisterEditorPanels(SceneId::Game, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "プレイヤーの状態情報", [this]() { player_->DrawImGui(); }, SceneId::Game, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "プレイヤーカメラ",
      [this]() {
        if (player_->GetPlayerCamera())
          player_->GetPlayerCamera()->DrawImGui();
      },
      SceneId::Game, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "ライティング",
      [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); },
      SceneId::Game, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "プレイヤー攻撃エディター", [this]() { attackEditor_->DrawImGui(); },
      SceneId::Game, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "魔法攻撃エディター",
      [this]() {
        // 魔法調整はプレイヤー状態確認とは作業目的が違う。
        // Editor
        // の独立パネルとして登録し、状態ウィンドウの開閉に引きずられないようにする。
        if (player_ && player_->GetMagicController()) {
          player_->GetMagicController()->DrawEditorWindow();
        }
      },
      SceneId::Game, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "YoRigine:パーティクルエディター",
      [this]() { YParticleEditor::GetInstance().ShowEditorWindow(); }, SceneId::Game, "ゲームプレイ");
#endif

  //------------------------------------------------------------
  // BGM再生
  //------------------------------------------------------------

  // static auto bgm = YoRigine::Audio::GetInstance()->Play(
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
  Player *player = player_.get();

  //------------------------------------------------------------
  // ゲームオーバー時の選択処理
  //------------------------------------------------------------
  bool isPlayerDead = player->GetCombat()->IsDead();

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
  if (player_->GetPlayerCamera())
    player_->GetPlayerCamera()->SetIsCloseUp(isPlayerDead);
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

  //------------------------------------------------------------
  // スティック入力・ロックオンを確定
  //------------------------------------------------------------
  if (player_->GetPlayerCamera()) {
    player_->GetPlayerCamera()->UpdatePreDirector();
  }

  //------------------------------------------------------------
  // サブシーン更新（プレイヤー・敵など全オブジェクトを動かす）
  //------------------------------------------------------------
  if (subSceneManager_) {
    subSceneManager_->Update();
  }

  //------------------------------------------------------------
  // 動いたプレイヤー位置をもとにカメラ計算
  //------------------------------------------------------------
  UpdateCamera();

  //------------------------------------------------------------
  //  攻撃カメラオフセットを sceneCamera に後付けで適用
  //------------------------------------------------------------
  if (player_->GetPlayerCamera()) {
    player_->GetPlayerCamera()->ApplyPostDirector(
        cameraRig_.GetCamera(), YoRigine::GameTime::GetDeltaTime());
  }

  // 空と共通システム更新
  skyBox_->Update();
  // バトル中のみ LB/RB の押下アニメを許可
  const bool inBattle =
      subSceneManager_ &&
      subSceneManager_->GetCurrentSceneName() == SubSceneId::Battle;
  gameUI_->SetBattleActive(inBattle);
  // 戦闘中は RB/LB
  // を「押しっぱなしで左右カメラ回転」に切り替える（非戦闘は背後リセンター）。
  // 攻撃/ガード入力も戦闘中のみ受け付ける。
  if (player_) {
    player_->SetBattleMode(inBattle);
    gameUI_->SetPlayerStyle(player_->GetStyle());
    if (player_->GetPlayerCamera()) {
      player_->GetPlayerCamera()->SetBattleMode(inBattle);
    }
  }

  // 画面外ヒット演出が発火していたら、ロックオン照準フラッシュを敵位置で再生
  {
    Vector3 flashWorld;
    if (player_ && player_->GetPlayerCamera() &&
        player_->GetPlayerCamera()->ConsumeLockOnFlashRequest(flashWorld)) {
      lockFlashWorldPos_ = flashWorld;
      lockFlashTimer_ = 1.7f; // アニメ(アルファ1.6s)＋余裕。この間は敵に追従
      lockFlashActive_ = true;
      gameUI_->FlashLockOn();
    }
    // 演出中は敵ワールド座標をスクリーンへ投影してフラッシュを追従させる
    // （カメラが敵方向へ回り込むのに合わせて照準が敵へ寄っていく）
    if (lockFlashActive_) {
      auto proj = Coordinate::WorldToScreen(
          lockFlashWorldPos_, cameraRig_.GetCamera()->GetViewProjectionMatrix());
      if (proj.has_value()) {
        gameUI_->SetLockOnFlashPos(proj->screenPos);
      }
      lockFlashTimer_ -= YoRigine::GameTime::GetUnscaledDeltaTime();
      if (lockFlashTimer_ <= 0.0f)
        lockFlashActive_ = false;
    }
  }

  gameUI_->Update();

  // 当たり判定・パーティクル・VFX・影行列などの共通更新は SceneManager が行う

#ifdef USE_IMGUI
  AreaEditor::GetInstance()->Update();
#endif // USE_IMGUI
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

  //------------------------------------------------------------
  // パーティクル描画
  //------------------------------------------------------------
  YParticleManager::GetInstance().Draw();
  DrawLine();
  // YoRigine::YGpuEmitManager::GetInstance()->Draw();

  //------------------------------------------------------------
  // VFX描画
  //------------------------------------------------------------
  VfxMeshSpawner::GetInstance()->Draw();
  player_->DrawVfx();
}

/// <summary>
/// PiP 用の 3D-only 描画 (スカイ + シーンオブジェクトのみ)。
/// PipCameraSystem がシーンカメラの行列を PiP
/// カメラの値に書き換えた状態で呼び出されるので、 ここでは普段の DrawObject
/// 経路と同じものを呼べば PiP 視点で再描画される。 UI / ライン /
/// ポストエフェクト / シャドウは含めない。
/// </summary>
void GameScene::DrawScene3DOnly() {
  skyBox_->Draw();
  Object3dCommon::GetInstance()->DrawPreference();
  DrawObject();

  DrawLine();
}

/// <summary>
/// オフスクリーン対象外のUI描画
/// </summary>
void GameScene::DrawNonOffscreen() {
  SpriteCommon::GetInstance()->DrawPreference();
  //------------------------------------------------------------
  // UI描画
  //------------------------------------------------------------
  SpriteCommon::GetInstance()->DrawPreference();
  DrawUI();
  if (subSceneManager_) {
    subSceneManager_->DrawNonOffscreen();
  }
}

/// <summary>
/// サブシーンのオブジェクト描画
/// </summary>
void GameScene::DrawObject() {
  if (subSceneManager_) {
    subSceneManager_->DrawObject();
  }
  YoRigine::SceneEditor::GetInstance()->Draw();
}

/// <summary>
/// サブシーンのライン描画
/// </summary>
void GameScene::DrawLine() {

  YoRigine::SceneEditor::GetInstance()->DrawLine();
  if (subSceneManager_) {
    subSceneManager_->DrawLine();
  }
}

/// <summary>
/// UI描画（現在空）
/// </summary>
void GameScene::DrawUI() {
  // gameUI_->DrawAll();
  gameUI_->Draw();
  if (subSceneManager_) {
    subSceneManager_->DrawUI();
  }
}

/// <summary>
/// カメラ更新処理
/// </summary>
void GameScene::UpdateCamera() {
  // 死亡モーション終了時はゲーム本体だけを止め、死亡クローズアップは
  // 実時間で最終位置まで進める。
  const bool isDeathCamera =
      player_ && player_->GetCombat() && player_->GetCombat()->IsDead();
  cameraRig_.Update(isDeathCamera);
}

/// <summary>
/// サブシーンからの通知
/// </summary>
void GameScene::HandleSubSceneEvent(SubSceneEvent event) {
  switch (event) {
  case SubSceneEvent::GameClear:
    HandleGameClear();
    break;
  }
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

  // フィールドを最初からやり直して切り替える
  if (subSceneManager_) {
    if (auto *fieldScene = subSceneManager_->GetScene(SubSceneId::Field)) {
      fieldScene->Restart();
    }
    subSceneManager_->SwitchToSceneWithTransition(SubSceneId::Field);
  }

  // デバッグカメラを解除してから、死亡で止まっていたゲーム時間を再開する
  cameraRig_.SetDebugActive(false);
  YoRigine::GameTime::Resume();

  Logger("[GameScene] Field Scene restarted\n");
}

/// <summary>
/// タイトルへ戻る処理
/// </summary>
void GameScene::HandleReturnToTitle() {
  Logger("[GameScene] Return to Title requested\n");

  // フェード中もゲームが進むよう、デバッグカメラによる停止を外しておく
  cameraRig_.SetDebugActive(false);

  // タイトルシーンへ遷移
  sceneManager_->ChangeScene(SceneId::Title);
  Logger("[GameScene] Changing to Title Scene\n");
}

/// <summary>
/// ゲームクリア処理
/// </summary>
void GameScene::HandleGameClear() {
  Logger("[GameScene] Game Clear! All enemies defeated!\n");

  // フェード中もゲームが進むよう、デバッグカメラによる停止を外しておく
  cameraRig_.SetDebugActive(false);

  // クリアシーンへ遷移
  sceneManager_->ChangeScene(SceneId::Clear);
  Logger("[GameScene] Changing to Clear Scene\n");
}

/// <summary>
/// 終了処理
/// </summary>
void GameScene::Finalize() {
  cameraRig_.Finalize();
  // メインシーン遷移前にパーティクルを停止する（Title/Clear への移動ケース）
  YParticleManager::GetInstance().StopAndClearActiveEmitters();
  YoRigine::YGpuEmitManager::GetInstance()->StopAllEmitterGroups();
  YoRigine::JsonManager::ClearSceneInstances("GameScene");

  // スポットライトへ渡したカメラと位置取得の関数は、このシーンの寿命に紐づく。
  // シングルトン側に残すと解放済みのシーンを参照してしまうため必ず外す。
  auto *spotlight = YoRigine::TutorialSpotlight::GetInstance();
  spotlight->ClearWorldTargets();
  spotlight->SetCamera(nullptr);

  if (subSceneManager_)
    subSceneManager_->Finalize();
  subSceneManager_ = nullptr;
}
