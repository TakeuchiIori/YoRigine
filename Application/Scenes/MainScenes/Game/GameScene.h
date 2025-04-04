#pragma once

// C++
#include <memory>
#include <map>
#include <d3d12.h>
#include <wrl.h>
#include <vector>

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
#include "Systems/MapChip/MapChipInfo.h"
#include "Systems/UI/UIBase.h"
#include "Enemy/EnemyManager.h"


// Cameras
#include "../../../SystemsApp/Cameras/DebugCamera/DebugCamera.h"
#include "../../../SystemsApp/Cameras/FollowCamera/FollowCamera.h"
#include "../../../SystemsApp/Cameras/TopDownCamera/TopDownCamera.h"

// Math
#include "Vector3.h"
#include "Quaternion.h"
#include <Enemy/EnemyManager.h>

enum class CameraMode {
    DEFAULT,
    FOLLOW,
    TOP_DOWN,
    DEBUG
};

class GameScene : public BaseScene {
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

    /// <summary>
    /// オフスクリーンに移したくないものの描画
    /// </summary>
    void DrawOffScreen() override;

    /// <summary>
    /// ビュープロジェクション取得
    /// </summary>
    /// <returns></returns>
    Matrix4x4 GetViewProjection() override { return sceneCamera_->viewProjectionMatrix_; }

private:
    /// <summary>
    /// 3Dオブジェクトの描画
    /// </summary>
    void DrawObject();

    /// <summary>
    /// 2Dスプライトの描画
    /// </summary>
    void DrawSprite();

    /// <summary>
    /// アニメーション描画
    /// </summary>
    void DrawAnimation();

    /// <summary>
    /// 線描画
    /// </summary>
    void DrawLine();

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

private:
    /*=================================================================

                               カメラ関連

    =================================================================*/
    CameraMode cameraMode_;
    std::shared_ptr<Camera> sceneCamera_;
    CameraManager cameraManager_;
    FollowCamera followCamera_;
    TopDownCamera topDownCamera_;
	DebugCamera debugCamera_;
	bool isDebugCamera_ = false;
    /*=================================================================

                               サウンド関連

    =================================================================*/
    Audio::SoundData soundData;
    IXAudio2SourceVoice* sourceVoice;

    /*=================================================================

                              パーティクル関連

    =================================================================*/
    std::unique_ptr<ParticleEmitter> particleEmitter_[2];
    Vector3 emitterPosition_;
    uint32_t particleCount_;

    /*=================================================================

                               スプライト関連

    =================================================================*/
    Vector3 weaponPos;
    std::unique_ptr<Sprite> sprite_;
    std::vector<std::unique_ptr<Sprite>> sprites;

    /*=================================================================

                               オブジェクト関連

    =================================================================*/
    std::unique_ptr<Object3d> test_;
    WorldTransform testWorldTransform_;
    std::unique_ptr<Player> player_;
    std::unique_ptr<Ground> ground_;
	std::unique_ptr<EnemyManager> enemyManager_;


    /*=================================================================

                                   線

    =================================================================*/
    std::unique_ptr<Line> line_;
    std::unique_ptr<Line> boneLine_;
    Vector3 start_ = { 0.0f, 0.0f, 0.0f };
    Vector3 end_ = { 10.0f, 0.0f, 10.0f };
    Sphere sphere_ =
    {
        Vector3(0.0f, 0.0f, 0.0f),
        5.0f
    };

	AABB aabb = {
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(5.0f, 5.0f, 5.0f)
	};

    OBB obb;
    // これをグローバルまたはクラス内に保持（毎フレーム初期化しない）
    Vector3 eulerDeg = { 0, 0, 0 }; // 度数（初期値）


    /*=================================================================

                                 その他

    =================================================================*/
    // コマンドパターン
    std::unique_ptr<InputHandleMove> inputHandler_ = nullptr;
    ICommandMove* iCommand_ = nullptr;

    bool isClear_ = false;

    std::unique_ptr<MapChipInfo> mpInfo_;
	std::unique_ptr<UIBase> uiBase_;
    std::unique_ptr<UIBase> uiSub_;

private:


};
