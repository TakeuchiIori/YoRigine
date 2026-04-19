#pragma once
#include "CameraState.h"
#include <vector>

// ============================================================
// 制御点構造体
// ============================================================
struct CameraControlPoint {
	Vector3 position;
	Vector3 rotation;
	float fov;
	float arrivalTime;

	CameraControlPoint(
		const Vector3& pos = { 0,0,0 },
		const Vector3& rot = { 0,0,0 },
		float f = 0.45f,
		float time = 1.0f)
		: position(pos), rotation(rot), fov(f), arrivalTime(time) {
	}
};

// ============================================================
// 制御点を使ったカメラワークステート（シネマティック演出用）
// ============================================================
class CinematicCameraState : public CameraState {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Enter(FollowCamera* camera) override;
	void Update(FollowCamera* camera) override;
	void Exit(FollowCamera* camera) override;

	bool IsFinished() const override { return isFinished_; }
	const char* GetStateName() const override { return "Cinematic"; }
	bool IsPerformance() const override { return true; }

	// ============================================================
	// 制御点の設定
	// ============================================================
	void AddControlPoint(const CameraControlPoint& point);
	void ClearControlPoints();
	void SetReturnInterpTime(float time) { returnInterpTime_ = time; }
	void SetLookAtTarget(bool enable) { lookAtTarget_ = enable; }

	// ============================================================
	// セーブ・ロード・GUI
	// ============================================================
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;
	void DrawEditGui() override;

	// ============================================================
	// 制御点の取得
	// ============================================================
	std::vector<CameraControlPoint>& GetControlPoints() { return controlPoints_; }
	const std::vector<CameraControlPoint>& GetControlPoints() const { return controlPoints_; }

protected:
	// ============================================================
	// 内部処理
	// ============================================================
	void InterpolateControlPoints(FollowCamera* camera);

	std::vector<CameraControlPoint> controlPoints_;
	int currentPointIndex_ = 0;
	float pointTimer_ = 0.0f;
	bool isFinished_ = false;

	float returnInterpTime_ = 0.5f;
	bool isReturning_ = false;
	float returnTimer_ = 0.0f;
	Vector3 returnStartPos_;
	Vector3 returnStartRot_;
	float returnStartFov_;

	bool lookAtTarget_ = false;
};