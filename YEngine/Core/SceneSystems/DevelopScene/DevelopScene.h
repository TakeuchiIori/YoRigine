#pragma once
// Engine
#include <SceneSystems/BaseScene.h>
#include <Systems/Camera/CameraMode.h>

#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/CameraEditor.h"
#include "Systems/Camera/Camera.h"
#include "Systems/Camera/CameraManager.h"

#include "Motion/MotionEditor.h"

class DevelopScene : public BaseScene
{
public:
	///************************* 基本関数 *************************///
	DevelopScene() : BaseScene("Develop") {}
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

	void UpdateCamera();

private:
	///************************* メンバ変数 *************************///

	// 出力用カメラ（実体）
	std::unique_ptr<Camera> sceneCamera_;
	CameraMode cameraMode_ = CameraMode::DEBUG;
	std::unique_ptr<CameraEditor> cameraEditor_;

	// デバッグフラグ
	bool isDebugCamera_ = false;

	std::unique_ptr<MotionEditor> motionEditor_ = nullptr;

};

