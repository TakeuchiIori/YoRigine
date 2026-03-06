#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>

class TitleCamera : public VirtualCamera
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	// エディタでの保存・読み込み用
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;


	bool  enableOrbit_ = false;
	void SetTarget(const WorldTransform& target) { target_ = &target; }
	void SetPosition(const Vector3& position) { transform_.translate = position; }
	Vector3 GetPosition() const { return transform_.translate; }
	float GetFov() const { return (fov_ >= 110.0f) ? fov_ : fov_; }

private:
	///************************* メンバ変数 *************************///
	const WorldTransform* target_;

	Vector2 prevMousePos_ = { 0.0f, 0.0f };
	float fov_ = 0.90f;


	// 回り込みカメラ用パラメータ
	float orbitRadius_ = 25.0f;   // プレイヤーからの距離
	float orbitSpeed_ = 0.3f;    // 回転速度
	float orbitHeight_ = 4.0f;    // 高さ
	float orbitAngle_ = 0.0f;    // 現在の角度
};