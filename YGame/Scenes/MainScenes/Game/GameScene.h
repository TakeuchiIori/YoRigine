#pragma once

// C++
#include <memory>
#include <functional>

// Engine
#include <SceneSystems/BaseScene.h>
#include "Systems/Audio/Audio.h"
#include "Particle/ParticleManager.h"
#include "CubeMap/CubeMap.h"
#include "GPUParticle/GPUEmitter.h"

#include "Object3D/Object3D.h"

// Camera
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/CameraEditor.h"
#include "Systems/Camera/Camera.h"
#include "Systems/Camera/CameraManager.h"

#include "Particle/YParticleEmitter.h"
// App
#include "../../SubScenes/SubSceneManager.h"
#include "../../SubScenes/FieldScene.h"
#include "../../SubScenes/BattleScene.h"
#include "../../SubScenes/SceneDataStructures.h"
#include "SkyBox/SkyBox.h"
#include "../UI/GameUI.h"
#include "../../../GameObjects/Player/Combo/AttackEditor.h"

// Math
#include "Vector3.h"

/// <summary>
/// ゲームシーン
/// </summary>
class GameScene : public BaseScene {
public:
	///************************* 基本関数 *************************///
	GameScene() : BaseScene("Game") {}
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawNonOffscreen() override;
	void DrawShadow() override;
	void Finalize() override;

	// BaseSceneインターフェース
	Matrix4x4 GetViewProjection() override { return sceneCamera_->viewProjectionMatrix_; }

private:
	///************************* 内部処理 *************************///

	void DrawObject();
	void DrawLine();
	void DrawUI();

	void UpdateCamera();
	void UpdateCameraMode();

	void HandleRetry();
	void HandleReturnToTitle();
	void HandleGameClear();

private:
	///************************* メンバ変数 *************************///


	// サブシーン管理
	std::unique_ptr<SubSceneManager> subSceneManager_;

	// 出力用カメラ（実体）
	std::unique_ptr<Camera> sceneCamera_;
	CameraMode cameraMode_;


	// プレイヤー（カメラの追従対象）
	std::unique_ptr<Player> player_;
	std::unique_ptr<CameraEditor> cameraEditor_;

	// デバッグフラグ
	bool isDebugCamera_ = false;

	std::unique_ptr<SkyBox> skyBox_;

	// サウンド関連
	YoRigine::Audio::SoundData soundData;
	IXAudio2SourceVoice* sourceVoice;

	// UI
	std::unique_ptr<GameUI> gameUI_;
#ifdef USE_IMGUI
	std::unique_ptr<AttackDataEditor> attackEditor_;
#endif


	// ゲームオーバー状態管理
	bool wasPlayerDead_ = false;

	// クリア状態管理
	bool isGameCleared_ = false;
	bool wasGameCleared_ = false;

	std::unique_ptr<YParticleEmitter> explosionEmitter_;
	Vector3 currentPosition_ = {0,3,0};
};