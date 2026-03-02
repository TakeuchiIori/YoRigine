#pragma once
// C++
#include <memory>
#include <map>

// Engine
#include <SceneSystems/BaseScene.h>
#include "Systems/Camera/Camera.h"
#include "Systems/Camera/CameraManager.h"


#include "../../../SystemsApp/Cameras/TopDownCamera/TopDownCamera.h"
#include "../../../SystemsApp/Cameras/SplineCamera/SplineCamera.h"
#include "../../../SystemsApp/Cameras/DefaultCamera/DefaultCamera.h"


#include "Systems/Audio/Audio.h"
#include "Particle/ParticleEmitter.h"
#include "Object3D/Object3d.h"
#include "Player/Player.h"
#include "WorldTransform./WorldTransform.h"
#include "Drawer/LineManager/Line.h"
#include "Player/DemoPlayer.h"
#include "SkyBox/SkyBox.h"
#include "Ground/Ground.h"
#include "../UI/TitleUI.h"

// Math
#include "Vector3.h"


/// <summary>
/// タイトルシーン
/// </summary>
class TitleScene : public BaseScene
{
	// カメラモード
	enum class CameraMode {
		DEFAULT,
		FOLLOW,
		TOP_DOWN,
		SPLINE,
		DEBUG,
	};

public:


	///************************* 基本関数 *************************///
	TitleScene() : BaseScene("Title") {}
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

	// カメラ
	CameraMode cameraMode_;
	std::shared_ptr<Camera> sceneCamera_;
	CameraManager cameraManager_;

	TopDownCamera topDownCamera_;
	SplineCamera splineCamera_;
	DefaultCamera defaultCamera_;
	bool isDebugCamera_ = false;

	// サウンド
	YoRigine::Audio::SoundData soundData;
	IXAudio2SourceVoice* sourceVoice;

	std::unique_ptr<DemoPlayer> player_;
	std::unique_ptr<SkyBox> skyBox_;
	std::unique_ptr<Ground> ground_;
	std::unique_ptr<TitleUI> titleUI_;

};

