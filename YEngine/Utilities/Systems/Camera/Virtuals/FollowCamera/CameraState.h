#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include "WorldTransform/WorldTransform.h"
#include "json.hpp"

class FollowCamera;

// ============================================================
// カメラステートの基底クラス
// 各カメラの演出や状態遷移の基本となるインターフェース
// ============================================================
class CameraState {
public:
	virtual ~CameraState() = default;

	// ============================================================
	// 各ステートで実装する主要関数
	// ============================================================
	virtual void Enter(FollowCamera* camera) = 0;
	virtual void Update(FollowCamera* camera) = 0;
	virtual void Exit(FollowCamera* camera) = 0;

	// ============================================================
	// 状態取得
	// ============================================================
	virtual bool IsFinished() const { return false; }
	virtual const char* GetStateName() const = 0;
	virtual bool IsPerformance() const { return false; }

	// ============================================================
	// セーブ・ロード・GUI
	// ============================================================
	virtual void Save([[maybe_unused]] nlohmann::json& j) const {}
	virtual void Load([[maybe_unused]] const nlohmann::json& j) {}
	virtual void DrawEditGui() {}

protected:
	float stateTimer_ = 0.0f;
};