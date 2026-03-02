#pragma once
#include "../VirtualCamera.h"

/// <summary>
/// デバッグカメラクラス（デバッグ用のカメラ）
/// </summary>
class DebugCamera : public VirtualCamera
{
public:
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	// エディタでの保存・読み込み用
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

private:
	void UpdateInput();

private:
	///************************* メンバ変数 *************************///
	// 入力状態
	Vector2 prevMousePos_ = { 0.0f, 0.0f };
	bool isDragging_ = false;
	bool isMoving_ = false;
	// 調整パラメータ
	float rotateSpeed_ = 0.05f;
	float rotateSpeedController_ = 0.005f;
	float moveSpeed_ = 0.5f;
	float moveSpeedController_ = 0.1f;
};