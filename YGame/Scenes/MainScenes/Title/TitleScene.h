#pragma once
// C++
#include <memory>

// Engine
#include "Systems/Camera/SceneCameraRig.h"
#include <SceneSystems/BaseScene.h>

// App
#include "../UI/TitleUI.h"
#include "Ground/Ground.h"
#include "Player/DemoPlayer.h"
#include "SkyBox/SkyBox.h"

/// <summary>
/// タイトルシーン
/// </summary>
class TitleScene : public BaseScene {
public:
  ///************************* 基本関数 *************************///
  TitleScene();
  void Initialize() override;
  void Update() override;
  void Draw() override;
  void DrawNonOffscreen() override;
  void Finalize() override;

  // PiP 用: シーンカメラ公開 + 3D オンリー描画パス
  YoRigine::Camera *GetSceneCamera() override { return cameraRig_.GetCamera(); }
  void DrawScene3DOnly() override;

private:
  ///************************* 内部処理 *************************///

  void DrawObject();

private:
  ///************************* メンバ変数 *************************///

  // 出力カメラ一式
  SceneCameraRig cameraRig_;

  std::unique_ptr<DemoPlayer> player_;
  std::unique_ptr<SkyBox> skyBox_;
  std::unique_ptr<Ground> ground_;
  std::unique_ptr<TitleUI> titleUI_;
};
