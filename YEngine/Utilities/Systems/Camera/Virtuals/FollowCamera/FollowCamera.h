#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>
#include "CameraState.h"
/// <summary>
/// デバッグカメラクラス(ターゲットを追従するカメラ)
/// </summary>
class FollowCamera : public VirtualCamera
{
public:
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	// セッター
	void SetTarget(const WorldTransform* target, const std::string& name) {
		target_ = target;
		targetName_ = name;
	}
	void SetIsCloseUp(bool isCloseUp) { isCloseUp_ = isCloseUp; }
	const std::string& GetTargetName() const { return targetName_; }

	// JSON用のSave/Loadオーバーライド
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	const WorldTransform* GetTarget()  { return target_; }
	void ChangeState(std::unique_ptr<CameraState> newState);

	void GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const;

	CameraState* GetCurrentState() const { return currentState_.get(); }
	void UpdateInput();
	void FollowProcess();
private:


private:
	const WorldTransform* target_ = nullptr;
	std::string targetName_ = ""; // 保存用の文字列
	
	std::unique_ptr<CameraState> currentState_ = nullptr;

	// 追従パラメータ
	Vector3 offset_ = { 0.0f, 6.0f, -40.0f };
	float kRotateSpeed_ = 0.1f;

	// クローズアップ関連
	bool isCloseUp_ = false;
	float closeUpScale_ = 0.3f;
	float interpSpeed_ = 5.0f;
	float currentScale_ = 1.0f;

	float kDeadZoneL_ = 100.0f;

	// カメラシェイク
	Vector3 shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	float shakeIntensity_ = 0.0f;
	float shakeDuration_ = 0.0f;
	float shakeTimer_ = 0.0f;

};