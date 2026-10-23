#pragma once

// C++
#include <functional>
#include <memory>

// Engine
#include "CubeMap/CubeMap.h"
#include "GPUParticle/YGpuEmitter.h"
#include "Systems/Audio/Audio.h"
#include <SceneSystems/BaseScene.h>

#include "Object3D/Object3D.h"

// Camera
#include "Systems/Camera/SceneCameraRig.h"

// App
#include "../../../GameObjects/Player/Combo/AttackEditor.h"
#include "../../SubScenes/SceneDataStructures.h"
#include "../../SubScenes/SubSceneManager.h"
#include "../UI/GameUI.h"
#include "SkyBox/SkyBox.h"

// Math
#include "Vector3.h"

/// <summary>
/// ゲームシーン
/// </summary>
class GameScene : public BaseScene {
public:
  ///************************* 基本関数 *************************///
  GameScene();
  void Initialize() override;
  void Update() override;
  void Draw() override;
  void DrawNonOffscreen() override;
  void Finalize() override;

  // PiP 用: シーンカメラのポインタを公開 + 3D だけ描画する軽量パス
  YoRigine::Camera *GetSceneCamera() override { return cameraRig_.GetCamera(); }
  void DrawScene3DOnly() override;

private:
  ///************************* 内部処理 *************************///

  void DrawObject();
  void DrawLine();
  void DrawUI();

  void UpdateCamera();

  // サブシーンからの通知を受け取る
  void HandleSubSceneEvent(SubSceneEvent event);

  void HandleRetry();
  void HandleReturnToTitle();
  void HandleGameClear();

private:
  ///************************* メンバ変数 *************************///

  // サブシーン管理
  std::unique_ptr<SubSceneManager> subSceneManager_;

  // 出力カメラ一式（通常はプレイヤー追従、F3 でデバッグカメラ）
  SceneCameraRig cameraRig_;

  // プレイヤー（カメラの追従対象）
  std::unique_ptr<Player> player_;

  std::unique_ptr<SkyBox> skyBox_;

  // UI
  std::unique_ptr<GameUI> gameUI_;
#ifdef USE_IMGUI
  std::unique_ptr<AttackDataEditor> attackEditor_;
#endif

  // ゲームオーバー状態管理
  bool wasPlayerDead_ = false;

  // ロックオン照準フラッシュ（画面外ヒット時・敵ワールド座標を投影して追従）
  bool lockFlashActive_ = false;
  float lockFlashTimer_ = 0.0f;
  Vector3 lockFlashWorldPos_ = {};
};
