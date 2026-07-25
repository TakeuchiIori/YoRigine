#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>

// ============================================================
// タイトル画面用のカメラクラス
// ターゲットを中心にしたオービット（回り込み）演出などを行う
// ============================================================
class TitleCamera : public VirtualCamera
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
	void SetTarget(const YoRigine::WorldTransform& target) { target_ = &target; }
	void SetPosition(const Vector3& position) { transform_.translate = position; }
	Vector3 GetPosition() const { return transform_.translate; }
	float GetFov() const { return (fov_ >= 110.0f) ? fov_ : fov_; }

public:
	// ============================================================
	// メンバ変数
	// ============================================================
	bool enableOrbit_ = false;

private:
	const YoRigine::WorldTransform* target_;
	Vector2 prevMousePos_ = { 0.0f, 0.0f };
	float fov_ = 0.90f;

	// ------------------------------------------------------------
	// 回り込みカメラ用パラメータ
	// ------------------------------------------------------------
	float orbitRadius_ = 25.0f;
	float orbitSpeed_ = 0.3f;
	float orbitHeight_ = 4.0f;
	float orbitAngle_ = 0.0f;
};