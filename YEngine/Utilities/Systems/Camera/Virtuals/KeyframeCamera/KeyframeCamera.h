#pragma once
#include "../VirtualCamera.h"
#include "Easing.h"
#include <vector>

class KeyframeCamera : public VirtualCamera {
public:

	///************************* 構造体 *************************///
	struct Keyframe {
		float time;              // 再生時間（秒）
		Vector3 translate;
		Vector3 rotate;
		float fov;
		Easing::Function easing; // この区間で使用するイージング
	};

	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	// キーフレームの追加・削除
	void AddKeyframe(float time, const Vector3& pos, const Vector3& rot, float fov, Easing::Function easing);
	void SortKeyframes(); // 時間順に並び替え

private:
	std::vector<Keyframe> keyframes_;
	float timer_ = 0.0f;
	bool isPlaying_ = false;
	bool isLooping_ = true;
	float playbackSpeed_ = 1.0f;
};