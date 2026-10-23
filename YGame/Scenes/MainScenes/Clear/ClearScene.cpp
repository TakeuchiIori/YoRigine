#include "ClearScene.h"

// Engine
#include "Collision/Core/CollisionManager.h"
#include "LightManager/LightManager.h"
#include "Loaders/Json/JsonManager.h"
#include "Object3D/Object3dCommon.h"
#include "OffScreen/PostEffectManager.h"
#include "Particle/YParticleEditor.h"
#include "Particle/YParticleManager.h"
#include "SceneEditor/SceneEditor.h"
#include "Sprite/SpriteCommon.h"
#include "Systems./Input./Input.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include <Object3D/BaseObjectManager.h>
#include <SceneSystems/SceneManager.h>

// App
#include "GameIds.h"

ClearScene::ClearScene() : BaseScene(SceneId::Clear) {}

/// <summary>
/// 初期化処理
/// </summary>
void ClearScene::Initialize() {

  //------------------------------------------------------------
  // カメラ初期化
  //------------------------------------------------------------
  SceneCameraRig::Desc cameraDesc;
  cameraDesc.editorFilePath =
      "Resources/Json/VirtualCameraData/ClearScene.json";
  cameraDesc.editorPresetName = "Clear";
  cameraDesc.mainCameraId = CameraId::ClearCamera;
  // 旋回カメラは毎フレームスナップしてブレンドを掛けない（従来の見た目を維持）
  cameraDesc.snapMainEveryFrame = true;
  cameraRig_.Initialize(cameraDesc);
  YoRigine::Camera *sceneCamera = cameraRig_.GetCamera();

  PostEffectManager::GetInstance()->LoadPreset("ClearScene");

  //------------------------------------------------------------
  // システム初期化
  //------------------------------------------------------------
  YoRigine::GameTime::Initialize();
  YoRigine::JsonManager::SetCurrentScene("ClearScene");
  YoRigine::CollisionManager::GetInstance()->Initialize();
  YoRigine::SceneEditor::GetInstance()->LoadScene("ClearScene");
  YoRigine::SceneEditor::GetInstance()->SetCamera(sceneCamera);
  YParticleManager::GetInstance().SetCamera(sceneCamera);

  // BaseObjectManager にカメラを 1 回だけ渡しておく（以降の一括駆動で使用）
  BaseObjectManager::GetInstance()->SetCamera(sceneCamera);

  //------------------------------------------------------------
  // クリア画面スプライトの生成と設定
  //------------------------------------------------------------
  clearUI_ = std::make_unique<ClearUI>();
  clearUI_->Initialize();

  //------------------------------------------------------------
  // オブジェクトの生成
  //------------------------------------------------------------

  // デモ用プレイヤー（Title と同様に演出用として配置）
  player_ = std::make_unique<DemoPlayer>();
  player_->Initialize(sceneCamera);
  player_->SetMotion("PowerUp1");
  // 一括 Update/Draw/Shadow の対象に登録（所有は ClearScene のまま）
  BaseObjectManager::GetInstance()->Register(player_.get(), "Player");

  clearEffect_ = EffectHandle::Play("ClearScene", Vector3{0, 0, 0}, true, 1);

  // スカイボックス / Ground は個別描画のためマネージャには登録しない
  skyBox_ = std::make_unique<SkyBox>();
  skyBox_->Initialize(sceneCamera,
                      "Resources/DDS/vz_sinister_land_cubemap_ue.dds");

  ground_ = std::make_unique<Ground>();
  ground_->Initialize(sceneCamera);

#ifdef USE_IMGUI
  cameraRig_.RegisterEditorPanels(SceneId::Clear, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "ライティング",
      [this]() { YoRigine::LightManager::GetInstance()->ShowLightingEditor(); },
      SceneId::Clear, "ゲームプレイ");
  Editor::GetInstance()->RegisterGameUI(
      "YoRigine:パーティクルエディター",
      [this]() { YParticleEditor::GetInstance().ShowEditorWindow(); },
      SceneId::Clear, "ゲームプレイ");
#endif
}

/// <summary>
/// 終了処理
/// </summary>
void ClearScene::Finalize() {
  clearEffect_.Stop();
  cameraRig_.Finalize();
  YoRigine::JsonManager::ClearSceneInstances("ClearScene");
}

/// <summary>
/// 更新処理
/// </summary>
void ClearScene::Update() {
  if (YoRigine::Input::GetInstance()->IsPadPressed(0, GamePadButton::A)) {
    sceneManager_->ChangeScene(SceneId::Title);
  }
  cameraRig_.Update();
  clearUI_->Update();

  // 登録オブジェクトは一括 Update。スカイボックス / Ground は個別
  // 当たり判定・パーティクルなどの共通更新は SceneManager が行う
  UpdateObjects();
  skyBox_->Update();
  ground_->Update();
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
  YParticleManager::GetInstance().Draw();
}

/// <summary>
/// PiP 用の 3D-only 描画（スカイ + シーンオブジェクトのみ）
/// </summary>
void ClearScene::DrawScene3DOnly() {
  skyBox_->Draw();
  Object3dCommon::GetInstance()->DrawPreference();
  DrawObject();
}

/// <summary>
/// オフスクリーン外の描画処理
/// </summary>
void ClearScene::DrawNonOffscreen() {
  //------------------------------------------------------------
  // 2Dスプライト描画（UI はポスト適用後のバックバッファへ描く）
  // ※ OffScreen(HDR R16F) パス内で SRGB の Sprite PSO を使うと
  //   #613 RENDER_TARGET_FORMAT_MISMATCH になるため、Title/Game と同様
  //   ここ（DrawNonOffscreen）で描画する。
  //------------------------------------------------------------
  SpriteCommon::GetInstance()->DrawPreference();
  clearUI_->DrawAll();
}

/// <summary>
/// オブジェクト描画（地面・プレイヤー）
/// </summary>
void ClearScene::DrawObject() {
  // Ground は個別描画、登録オブジェクト（player_）は一括描画
  ground_->Draw();
  DrawObjects();
  YoRigine::SceneEditor::GetInstance()->Draw();
}
