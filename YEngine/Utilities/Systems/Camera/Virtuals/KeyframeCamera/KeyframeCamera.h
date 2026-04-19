#pragma once
#include "../VirtualCamera.h"
#include "Easing.h"
#include <vector>

// ============================================================
// キーフレームカメラクラス
// 時間軸に沿って複数の座標・回転・FOVを補間してアニメーションする
// ============================================================
class KeyframeCamera : public VirtualCamera {
public:
	// ============================================================
	// 構造体定義
	// ============================================================
	struct Keyframe {
		float time;
		Vector3 translate;
		Vector3 rotate;
		float fov;
		Easing::Function easing;
	};

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
	// キーフレーム操作
	// ============================================================
	void AddKeyframe(float time, const Vector3& pos, const Vector3& rot, float fov, Easing::Function easing);
	void SortKeyframes();

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	std::vector<Keyframe> keyframes_;
	float timer_ = 0.0f;
	bool isPlaying_ = false;
	bool isLooping_ = true;
	float playbackSpeed_ = 1.0f;
};