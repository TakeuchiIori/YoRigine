#pragma once
#include "../VirtualCamera.h"

// ============================================================
// デバ��グカメラクラス（開発中の自由な視点移動用カメラ）
// ============================================================
class DebugCamera : public VirtualCamera
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
	bool isMoving_ = false;

	float rotateSpeed_ = 0.05f;
	float rotateSpeedController_ = 0.005f;
	float moveSpeed_ = 0.5f;
	float moveSpeedController_ = 0.1f;
};