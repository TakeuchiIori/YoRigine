#pragma once
// C++
#include <memory>
#include <map>

// Engine
#include "CoreScenes/Base/BaseScene.h"
#include "Systems/Camera/Camera.h"
#include "Systems/Camera/CameraManager.h"
#include "Systems/Audio/Audio.h"
#include "Particle/ParticleEmitter.h"
#include "Object3D/Object3d.h"
#include "Sprite/Sprite.h"
#include "Player/Player.h"
#include "WorldTransform./WorldTransform.h"
#include "Drawer/LineManager/Line.h"
#include "../Transitions/Fade/Fade.h"

// Math
#include "Vector3.h"



class TitleScene : public BaseScene
{

	enum class CameraMode
	{
		DEFAULT,
		FOLLOW,
		TOP_DOWN,
		FPS

	};

public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 終了
	/// </summary>
	void Finalize() override;

	/// <summary>
	/// 更新
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
	void Draw() override;

private:


	/// <summary>
	/// カメラモードを更新する
	/// </summary>
	void UpdateCameraMode();

	/// <summary>
	/// カメラを更新する
	/// </summary>
	void UpdateCamera();



private:
	// カメラ
	CameraMode cameraMode_;
	std::shared_ptr<Camera> currentCamera_;
	CameraManager cameraManager_;
	// サウンド
	Audio::SoundData soundData;
	IXAudio2SourceVoice* sourceVoice;

	// プレイヤー
	std::unique_ptr<Player> player_;

	std::unique_ptr<Sprite> sprite_;
};

