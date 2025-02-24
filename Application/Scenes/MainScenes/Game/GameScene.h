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
#include "Enemy/Enemy.h"
#include "WorldTransform./WorldTransform.h"
#include "Drawer/LineManager/Line.h"
#include "Player/ICommand/ICommandMove.h"
#include "Player/InputHandle/InputHandleMove.h"
#include "Ground/Ground.h"
#include "../Transitions/Fade/Fade.h"
#include "OffScreen/OffScreen.h"

// Math
#include "Vector3.h"
#include "../../../SystemsApp/Cameras/FollowCamera/FollowCamera.h"
#include "../../../SystemsApp/Cameras/TopDownCamera/TopDownCamera.h"
#include <Enemy/EnemyManager.h>

enum class CameraMode
{
    DEFAULT,
    FOLLOW,
    TOP_DOWN,
    FPS

};

class GameScene : public BaseScene
{
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

    /// <summary>
    /// ImGui
    /// </summary>
    void ShowImGui();

    void CheckAllCollisions();


private:
    // カメラ
    CameraMode cameraMode_;
    std::shared_ptr<Camera> sceneCamera_;
    CameraManager cameraManager_;
	FollowCamera followCamera_;
    TopDownCamera topDownCamera_;
    // サウンド
    Audio::SoundData soundData;
    IXAudio2SourceVoice* sourceVoice;
    // パーティクルエミッター
    std::unique_ptr<ParticleEmitter> particleEmitter_[2];
    Vector3 emitterPosition_;
    uint32_t particleCount_;

    Vector3 weaponPos;

    std::unique_ptr<Sprite> sprite_;

    // 3Dモデル
    std::unique_ptr<Object3d> test_;
    WorldTransform testWorldTransform_;

	std::unique_ptr<OffScreen> offScreen_;

    // プレイヤー
    std::unique_ptr<Player> player_;

    // 地面
    std::unique_ptr< Ground> ground_;

    // 2Dスプライト
    std::vector<std::unique_ptr<Sprite>> sprites;

    // コマンドパターン
    std::unique_ptr<InputHandleMove> inputHandler_ = nullptr;
    ICommandMove* iCommand_ = nullptr;

    // Line
    std::unique_ptr<Line> line_;
    std::unique_ptr<Line> boneLine_;
    Vector3 start_ = { 0.0f,0.0f,0.0f };
    Vector3 end_ = { 10.0f,0.0f,10.0f };

     bool isClear_ = false;
 
};
