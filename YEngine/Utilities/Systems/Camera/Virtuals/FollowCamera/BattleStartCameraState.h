#pragma once
#include "CinematicCameraState.h"

// ============================================================
// 戦闘開始時のカメラワークステート
// ============================================================
class BattleStartCameraState : public CinematicCameraState {
public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Enter(FollowCamera* camera) override;
	const char* GetStateName() const override { return "BattleStart"; }
	bool IsPerformance() const override { return true; }

	// ============================================================
	// セーブ・ロード・GUI
	// ============================================================
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;
	void DrawEditGui() override;

	// ============================================================
	// 制御点管理
	// ============================================================
	void RebuildControlPoints(FollowCamera* camera);
	void SetupDefaultControlPoints(FollowCamera* camera);

private:
	void ApplyTargetOffset(FollowCamera* camera);

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	static constexpr int kPointCount = 3;

	Vector3 offsets_[kPointCount] = {
		{  0.0f, 15.0f, -20.0f },
		{ 10.0f,  5.0f,  -5.0f },
		{ -2.0f,  4.0f, -12.0f }
	};
	Vector3 rotations_[kPointCount] = {
		{ 0.6f,  0.0f, 0.0f },
		{ 0.2f, -0.8f, 0.0f },
		{ 0.1f,  0.0f, 0.0f }
	};
	float fovs_[kPointCount] = { 0.5f,  0.45f, 0.45f };
	float controlPointTimes_[kPointCount] = { 0.6f,  0.5f,  0.6f };

	float returnInterpTime_ = 0.5f;
	bool  lookAtTargetFlag_ = true;

	Vector3 lastTargetPos_ = { 0.0f, 0.0f, 0.0f };
};