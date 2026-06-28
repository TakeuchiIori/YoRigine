#pragma once
#include "CinematicCameraState.h"

// ============================================================
// パリィ成功時のカメラワークステート
// ============================================================
class ParryCameraState : public CinematicCameraState {
public:
	// パリィ演出のタイプ定義
	enum class ParryType {
		Quick,
		Dramatic,
		SlowMotion
	};

public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Enter(FollowCamera* camera) override;
	const char* GetStateName() const override { return "Parry"; }
	bool IsPerformance() const override { return true; }

	// ============================================================
	// パラメータアクセス
	// ============================================================
	void SetParryType(ParryType type) { parryType_ = type; }
	ParryType GetParryType() const { return parryType_; }

	// ============================================================
	// セーブ・ロード・GUI
	// ============================================================
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;
	void DrawEditGui() override;

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void SetupControlPoints(FollowCamera* camera);
	ParryType parryType_ = ParryType::Quick;
};