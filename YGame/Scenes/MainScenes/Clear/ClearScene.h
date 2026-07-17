#pragma once
// C++
#include <memory>
#include <map>

// Engine
#include <SceneSystems/BaseScene.h>
#include "Particle/EffectHandle.h"
#include "Systems/Audio/Audio.h"
#include "Object3D/Object3d.h"
#include "Sprite/Sprite.h"
#include "Player/Player.h"
#include "WorldTransform./WorldTransform.h"
#include "Drawer/LineManager/Line.h"
// Camera
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/CameraEditor.h"
#include "Systems/Camera/Camera.h"
#include "Systems/Camera/CameraMode.h"

// Math
#include "Vector3.h"

// App
#include "Player/DemoPlayer.h"
#include "SkyBox/SkyBox.h"
#include "Ground/Ground.h"
#include "../UI/ClearUI.h"

/// <summary>
/// クリアシーン
/// </summary>
class ClearScene : public BaseScene
{

public:
	///************************* 基本関数 *************************///
	ClearScene() : BaseScene("Clear") {}
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawNonOffscreen() override;
	void DrawShadow() override;
	void Finalize() override;
	Matrix4x4 GetViewProjection() override { return sceneCamera_->viewProjectionMatrix_; }

private:
	///************************* 内部処理 *************************///

	void DrawObject();
	void DrawLine();
	void DrawUI();

	void UpdateCamera();
	void UpdateCameraMode();
private:
	///************************* メンバ変数 *************************///

	// 出力用カメラ（実体）
	std::unique_ptr<Camera> sceneCamera_;
	CameraMode cameraMode_;
	std::unique_ptr<CameraEditor> cameraEditor_;
	bool isDebugCamera_ = false;


	// サウンド
	YoRigine::Audio::SoundData soundData;

	std::unique_ptr<DemoPlayer> player_;
	std::unique_ptr<SkyBox> skyBox_;
	std::unique_ptr<Ground> ground_;
	std::unique_ptr<ClearUI> clearUI_;

	EffectHandle clearEffect_;
};
