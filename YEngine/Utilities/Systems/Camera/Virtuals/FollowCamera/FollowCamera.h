#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>
#include "CameraState.h"
#include "BattleStartCameraState.h"
#include "../../CameraCollisionResolver.h"

// ============================================================
// デバッグカメラクラス(ターゲットを追従するカメラ)
// ============================================================
class FollowCamera : public VirtualCamera
{
public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	// ============================================================
	// アクセッサ・状態取得
	// ============================================================
	void SetTarget(const WorldTransform* target, const std::string& name) {
		target_ = target;
		targetName_ = name;
	}
	const std::string& GetTargetName() const { return targetName_; }
	const WorldTransform* GetTarget() { return target_; }

	void SetIsCloseUp(bool isCloseUp) { isCloseUp_ = isCloseUp; }
	CameraState* GetCurrentState() const { return currentState_.get(); }

	// ============================================================
	// カメラ制御・ステート管理
	// ============================================================
	void ChangeState(std::unique_ptr<CameraState> newState);
	void GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const;

	// カメラシェイクを開始する
	void StartShake(float intensity, float duration);

	void UpdateInput();
	void FollowProcess();

	// ============================================================
	// 戦闘開始演出
	// ============================================================
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

	bool IsInPerformance() const {
		if (currentState_) {
			return currentState_->IsPerformance();
		}
		return false;
	}

private:
	// ============================================================
	// ターゲット管理
	// ============================================================
	const WorldTransform* target_ = nullptr;
	std::string targetName_ = "";

	// ============================================================
	// ステート管理
	// ============================================================
	std::unique_ptr<CameraState> currentState_ = nullptr;
	std::shared_ptr<BattleStartCameraState> battleStartState_ = nullptr;

	// ============================================================
	// 追従パラメータ
	// ============================================================
	Vector3 offset_ = { 0.0f, 6.0f, -40.0f };
	float kRotateSpeed_ = 0.1f;

	// ============================================================
	// クローズアップ関連
	// ============================================================
	bool isCloseUp_ = false;
	float closeUpScale_ = 0.3f;
	float interpSpeed_ = 5.0f;
	float currentScale_ = 1.0f;

	// ============================================================
	// カメラシェイク
	// ============================================================
	void UpdateShake();

	Vector3 shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	float shakeIntensity_ = 0.0f;
	float shakeDuration_ = 0.0f;
	float shakeTimer_ = 0.0f;

	// ============================================================
	// カメラの回転制限（ラジアン）
	// ============================================================
	float minPitch_ = -0.2f; // 見上げの限界
	float maxPitch_ = 1.2f; // 見下ろしの限界

	// ============================================================
	// その他
	// ============================================================
	bool isPreviewMode_ = false;

	// 壁避け用コンポーネント
	CameraCollisionResolver collisionResolver_;
	float targetPivot_Height_ = 1.5f; // プレイヤーの頭あたりを注視点とするための高さオフセット
};