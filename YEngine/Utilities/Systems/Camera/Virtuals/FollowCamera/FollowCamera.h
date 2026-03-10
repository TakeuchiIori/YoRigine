#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>
#include "CameraState.h"
#include "BattleStartCameraState.h"

/// <summary>
/// デバッグカメラクラス(ターゲットを追従するカメラ)
/// </summary>
class FollowCamera : public VirtualCamera
{
public:
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	void SetTarget(const WorldTransform* target, const std::string& name) {
		target_ = target;
		targetName_ = name;
	}
	void SetIsCloseUp(bool isCloseUp) { isCloseUp_ = isCloseUp; }
	const std::string& GetTargetName() const { return targetName_; }

	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	const WorldTransform* GetTarget() { return target_; }
	void ChangeState(std::unique_ptr<CameraState> newState);
	void GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const;
	CameraState* GetCurrentState() const { return currentState_.get(); }

	/// 戦闘開始演出を発火（ゲームシーンから呼ぶだけでOK）
	void PlayBattleStart() {
		if (!battleStartState_) {
			battleStartState_ = std::make_shared<BattleStartCameraState>();
		}
		auto play = std::make_unique<BattleStartCameraState>();
		nlohmann::json j;
		battleStartState_->Save(j);
		play->Load(j);
		play->RebuildControlPoints(this);
		ChangeState(std::move(play));
	}

	void UpdateInput();
	void FollowProcess();

	// 現在のステートが戦闘開始演出中かどうか
	bool IsInPerformance() const {
		if (currentState_) {
			return currentState_->IsPerformance();
		}
		return false;
	}

private:
	const WorldTransform* target_ = nullptr;
	std::string targetName_ = "";
	std::unique_ptr<CameraState> currentState_ = nullptr;

	/// 戦闘開始ステートの設定を保持（編集・Save/Load用）
	std::shared_ptr<BattleStartCameraState> battleStartState_ = nullptr;

	// 追従パラメータ
	Vector3 offset_ = { 0.0f, 6.0f, -40.0f };
	float kRotateSpeed_ = 0.1f;

	// クローズアップ関連
	bool isCloseUp_ = false;
	float closeUpScale_ = 0.3f;
	float interpSpeed_ = 5.0f;
	float currentScale_ = 1.0f;

	// カメラシェイク
	Vector3 shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	float shakeIntensity_ = 0.0f;
	float shakeDuration_ = 0.0f;
	float shakeTimer_ = 0.0f;

	// プレビュー状態
	bool isPreviewMode_ = false;
};