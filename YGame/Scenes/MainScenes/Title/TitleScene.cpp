#include "TitleScene.h"

// Engine
#include "LightManager/LightManager.h"
#include "Loaders/Json/JsonManager.h"
#include "Object3D/Object3dCommon.h"
#include "OffScreen/PostEffectManager.h"
#include "Particle/YParticleManager.h"
#include "Sprite/SpriteCommon.h"
#include "Systems./Input./Input.h"
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/Virtuals/TitleCamera/TitleCamera.h"
#include <Collision/Core/CollisionManager.h>
#include <Editor/Editor.h>
#include <Object3D/BaseObjectManager.h>
#include <SceneEditor/SceneEditor.h>
#include <SceneSystems/SceneManager.h>
#include <Systems/GameTime/GameTime.h>

// App
#include "GameIds.h"

TitleScene::TitleScene() : BaseScene(SceneId::Title) {}

/// <summary>
/// 初期化処理
/// </summary>
void TitleScene::Initialize() {

  //------------------------------------------------------------
  // カメラ初期化
  //------------------------------------------------------------
  SceneCameraRig::Desc cameraDesc;
  cameraDesc.editorFilePath =
      "Resources/Json/VirtualCameraData/TitleScene.json";
  cameraDesc.editorPresetName = "Title";
  cameraDesc.mainCameraId = CameraId::TitleCamera;
  // 旋回カメラは毎フレームスナップしてブレンドを掛けない（従来の見た目を維持）
  cameraDesc.snapMainEveryFrame = true;
  cameraRig_.Initialize(cameraDesc);
  YoRigine::Camera *sceneCamera = cameraRig_.GetCamera();

  // タイトル用のポストエフェクトの追加（TestGame: Fog + GodRays + Bloom 等）
  PostEffectManager::GetInstance()->LoadPreset("TestGame");

  //------------------------------------------------------------
  // システム初期化
  //------------------------------------------------------------
  YoRigine::GameTime::Initialize();
  YoRigine::JsonManager::SetCurrentScene("TitleScene");
  YoRigine::CollisionManager::GetInstance()->Initialize();
  YoRigine::SceneEditor::GetInstance()->LoadScene("TitleScene");
  YoRigine::SceneEditor::GetInstance()->SetCamera(sceneCamera);

  YParticleManager::GetInstance().SetCamera(sceneCamera);

  // BaseObjectManager にカメラを 1 回だけ渡しておく（以降の一括駆動で使用）
  BaseObjectManager::GetInstance()->SetCamera(sceneCamera);
  //------------------------------------------------------------
  // タイトル専用要素の初期化
  //------------------------------------------------------------
  titleUI_ = std::make_unique<TitleUI>();
  titleUI_->Initialize();

  player_ = std::make_unique<DemoPlayer>();
  player_->Initialize(sceneCamera);
  player_->SetMotion("Idle1");
  // 一括 Update/Draw/Shadow の対象に登録（所有は TitleScene のまま）
  BaseObjectManager::GetInstance()->Register(player_.get(), "Player");

  auto titleCamera = std::dynamic_pointer_cast<TitleCamera>(
      CameraDirector::GetInstance()->GetCamera(CameraId::TitleCamera));
  if (titleCamera) {
    titleCamera->SetTarget(player_->GetWT());
    titleCamera->enableOrbit_ = true; // カメラ回転有効
  }

  // スカイボックスは最初に描く特殊オブジェクトのため、マネージャには登録せず個別描画する
  skyBox_ = std::make_unique<SkyBox>();
  skyBox_->Initialize(sceneCamera,
                      "Resources/DDS/vz_sinister_land_cubemap_ue.dds");

  // Ground は BaseObject
  // 非継承の独立クラスのため、マネージャには登録せず個別に扱う
  ground_ = std::make_unique<Ground>();
  ground_->Initialize(sceneCamera);

  //------------------------------------------------------------
  // エディター登録（デバッグ用）
  //------------------------------------------------------------
#ifdef USE_IMGUI
  cameraRig_.RegisterEditorPanels(SceneId::Title, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "ライティング",
      [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); },
      SceneId::Title, "ゲームプレイ");
#endif
}

/// <summary>
/// 更新処理
/// </summary>
void TitleScene::Update() {
  cameraRig_.Update();

  // スペースまたはAボタンでゲームシーンへ遷移
  if (YoRigine::Input::GetInstance()->PushKey(DIK_SPACE) ||
      YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::A)) {
    sceneManager_->ChangeScene(SceneId::Game);
  }

  // 各種更新（登録オブジェクトは一括 Update。スカイボックス / Ground は個別）
  // 当たり判定・パーティクルなどの共通更新は SceneManager が行う
  UpdateObjects();
  skyBox_->Update();
  ground_->Update();
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
  YoRigine::SceneEditor::GetInstance()->Draw();
}

/// <summary>
/// PiP 用の 3D-only 描画（スカイ + シーンオブジェクトのみ）
/// </summary>
void TitleScene::DrawScene3DOnly() { Draw(); }

/// <summary>
/// オフスクリーン外の描画処理
/// </summary>
void TitleScene::DrawNonOffscreen() {
  //------------------------------------------------------------
  // 2D UI描画
  //------------------------------------------------------------
  SpriteCommon::GetInstance()->DrawPreference();
  titleUI_->Draw();
}

/// <summary>
/// 解放処理
/// </summary>
void TitleScene::Finalize() {
  cameraRig_.Finalize();
  YoRigine::JsonManager::ClearSceneInstances("TitleScene");
}

/// <summary>
/// オブジェクト描画（地面・プレイヤー）
/// </summary>
void TitleScene::DrawObject() {
  // Ground は個別描画、登録オブジェクト（player_）は一括描画
  ground_->Draw();
  DrawObjects();
}
