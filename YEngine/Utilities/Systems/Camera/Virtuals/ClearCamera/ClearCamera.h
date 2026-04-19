#pragma once
#include "../VirtualCamera.h"

// ============================================================
// クリア演出時などに使用するカメラクラス
// ============================================================
class ClearCamera : public VirtualCamera
{
public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	// ============================================================
	// セーブ・ロード
	// ============================================================
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	// ============================================================
	// アクセッサ
	// ============================================================
	void SetIsMoving(bool isMoving) { isMoving_ = isMoving; }

private:
	// ============================================================
	// 内部処理
	// ============================================================
	void UpdateInput();

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	Vector2 prevMousePos_ = { 0.0f, 0.0f };
	bool isDragging_ = false;

	float rotateSpeed_ = 0.05f;
	float rotateSpeedController_ = 0.005f;
	float moveSpeed_ = 0.5f;
	float moveSpeedController_ = 0.1f;

	bool isMoving_ = false;
};