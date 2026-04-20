#include "KeyframeCamera.h"
#include "Systems/GameTime/GameTime.h"
#include <algorithm>
#include <imgui.h>
#include <Systems/Camera/CameraDirector.h>

// ============================================================
// 初期化
// ============================================================
void KeyframeCamera::Initialize() {
	VirtualCamera::Initialize();
	timer_ = 0.0f;
	isPlaying_ = false;
}

// ============================================================
// 更新処理
// ============================================================
void KeyframeCamera::Update() {
	if (!isPlaying_ || keyframes_.size() < 2) return;

	// ------------------------------------------------------------
	// 時間の進行とループ制御
	// ------------------------------------------------------------
	timer_ += (YoRigine::GameTime::GetDeltaTime()) * playbackSpeed_;

	float maxTime = keyframes_.back().time;
	if (timer_ > maxTime) {
		if (isLooping_) timer_ = fmod(timer_, maxTime);
		else {
			timer_ = maxTime;
			isPlaying_ = false;
		}
	}

	// ------------------------------------------------------------
	// 現在時刻のキーフレームの検索と補間計算
	// ------------------------------------------------------------
	for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
		auto& kStart = keyframes_[i];
		auto& kEnd = keyframes_[i + 1];

		if (timer_ >= kStart.time && timer_ <= kEnd.time) {
			float t = (timer_ - kStart.time) / (kEnd.time - kStart.time);
			float easedT = Easing::Ease(kStart.easing, t);

			transform_.translate = Lerp(kStart.translate, kEnd.translate, easedT);
			transform_.rotate = Lerp(kStart.rotate, kEnd.rotate, easedT);
			fovY_ = Lerp(kStart.fov, kEnd.fov, easedT);
			break;
		}
	}
}

// ============================================================
// エディタ用GUI描画
// ============================================================
void KeyframeCamera::DrawDebugGui() {
#ifdef USE_IMGUI
	ImGui::Text("--- キーフレームアニメーション ---");

	// ------------------------------------------------------------
	// 再生コントロール
	// ------------------------------------------------------------
	if (ImGui::Button(isPlaying_ ? "一時停止" : "再生")) isPlaying_ = !isPlaying_;
	ImGui::SameLine();
	if (ImGui::Button("リセット")) timer_ = 0.0f;

	ImGui::SliderFloat("再生時間", &timer_, 0.0f, keyframes_.empty() ? 0.0f : keyframes_.back().time);
	ImGui::Checkbox("ループ再生", &isLooping_);
	ImGui::DragFloat("再生速度", &playbackSpeed_, 0.1f, 0.0f, 5.0f);

	ImGui::Separator();

	// ------------------------------------------------------------
	// キーフレームの追加
	// ------------------------------------------------------------
	if (ImGui::Button("現在のアングルをキーとして追加")) {
		auto director = CameraDirector::GetInstance();
		float nextTime = keyframes_.empty() ? 0.0f : keyframes_.back().time + 2.0f;

		AddKeyframe(nextTime, director->GetActiveCameraPos(), director->GetActiveCameraRot(), director->GetFovY(), Easing::Function::EaseInOutQuad);
	}

	// ------------------------------------------------------------
	// キーフレームリストの表示と削除
	// ------------------------------------------------------------
	if (ImGui::TreeNode("キーフレーム一覧")) {
		for (int i = 0; i < keyframes_.size(); i++) {
			ImGui::PushID(i);
			ImGui::Text("Key %d:", i);
			ImGui::SameLine();
			ImGui::DragFloat("時間", &keyframes_[i].time, 0.1f);

			if (ImGui::Button("削除")) {
				keyframes_.erase(keyframes_.begin() + i);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
#endif
}

// ============================================================
// 保存
// ============================================================
void KeyframeCamera::Save(nlohmann::json& j) const {
	VirtualCamera::Save(j);

	j["isLooping"] = isLooping_;
	j["playbackSpeed"] = playbackSpeed_;

	j["keyframes"] = nlohmann::json::array();
	for (const auto& kf : keyframes_) {
		nlohmann::json kfJson;
		kfJson["time"] = kf.time;
		kfJson["translate"] = { kf.translate.x, kf.translate.y, kf.translate.z };
		kfJson["rotate"] = { kf.rotate.x, kf.rotate.y, kf.rotate.z };
		kfJson["fov"] = kf.fov;
		kfJson["easing"] = static_cast<int>(kf.easing);

		j["keyframes"].push_back(kfJson);
	}
}

// ============================================================
// 読み込み
// ============================================================
void KeyframeCamera::Load(const nlohmann::json& j) {
	VirtualCamera::Load(j);

	isLooping_ = j.value("isLooping", true);
	playbackSpeed_ = j.value("playbackSpeed", 1.0f);

	if (j.contains("keyframes") && j["keyframes"].is_array()) {
		keyframes_.clear();
		for (const auto& kfJson : j["keyframes"]) {
			Keyframe kf;
			kf.time = kfJson["time"];
			kf.translate = { kfJson["translate"][0], kfJson["translate"][1], kfJson["translate"][2] };
			kf.rotate = { kfJson["rotate"][0], kfJson["rotate"][1], kfJson["rotate"][2] };
			kf.fov = kfJson["fov"];
			kf.easing = static_cast<Easing::Function>(kfJson.value("easing", 0));

			keyframes_.push_back(kf);
		}
	}
	SortKeyframes();
}

// ============================================================
// キーフレームの追加
// ============================================================
void KeyframeCamera::AddKeyframe(float time, const Vector3& pos, const Vector3& rot, float fov, Easing::Function easing) {
	keyframes_.push_back({ time, pos, rot, fov, easing });
	SortKeyframes();
}

// ============================================================
// 時間順にソート
// ============================================================
void KeyframeCamera::SortKeyframes() {
	std::sort(keyframes_.begin(), keyframes_.end(), [](const Keyframe& a, const Keyframe& b) {
		return a.time < b.time;
		});
}