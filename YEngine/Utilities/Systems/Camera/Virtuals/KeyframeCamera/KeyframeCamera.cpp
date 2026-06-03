#include "KeyframeCamera.h"
#include "Systems/GameTime/GameTime.h"
#include <algorithm>
#include <imgui.h>
#include <Systems/Camera/CameraDirector.h>
#include "Systems/Camera/Virtuals/DebugCamera/DebugCamera.h"
#include "Drawer/LineManager/Line.h"

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
//   - 再生中なら時間進行
//   - 常に EvaluateAt(timer_) でカメラへ反映（スクラブ対応のため非再生時も評価）
// ============================================================
void KeyframeCamera::Update() {
	if (keyframes_.size() < 2) return;

	if (isPlaying_) {
		timer_ += (YoRigine::GameTime::GetDeltaTime()) * playbackSpeed_;

		float maxTime = keyframes_.back().time;
		if (timer_ > maxTime) {
			if (isLooping_) timer_ = fmod(timer_, maxTime);
			else {
				timer_ = maxTime;
				isPlaying_ = false;
			}
		}
	}

	EvaluateAt(timer_);
}

// ============================================================
// 指定時刻のキー補間を即時評価し、transform_ / fovY_ を更新する
// ============================================================
void KeyframeCamera::EvaluateAt(float time) {
	if (keyframes_.size() < 2) return;

	// 範囲外は端点クランプ
	float t0 = keyframes_.front().time;
	float t1 = keyframes_.back().time;
	float clampedTime = std::clamp(time, t0, t1);

	// 現在時刻を含むセグメントを検索
	for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
		auto& kStart = keyframes_[i];
		auto& kEnd   = keyframes_[i + 1];

		if (clampedTime >= kStart.time && clampedTime <= kEnd.time) {
			float segSpan = std::max(kEnd.time - kStart.time, 1e-6f);
			float t = (clampedTime - kStart.time) / segSpan;
			float easedT = Easing::Ease(kStart.easing, t);

			if (interpolationMode_ == InterpolationMode::CatmullRom
				&& keyframes_.size() >= 3) {
				const size_t last = keyframes_.size() - 1;
				const Vector3& p1 = kStart.translate;
				const Vector3& p2 = kEnd.translate;
				const Vector3 p0 = (i == 0)        ? (2.0f * p1 - p2) : keyframes_[i - 1].translate;
				const Vector3 p3 = (i + 1 == last) ? (2.0f * p2 - p1) : keyframes_[i + 2].translate;
				transform_.translate = CatmullRomInterpolation(p0, p1, p2, p3, easedT);

				const Vector3& r1 = kStart.rotate;
				const Vector3& r2 = kEnd.rotate;
				const Vector3 r0 = (i == 0)        ? (2.0f * r1 - r2) : keyframes_[i - 1].rotate;
				const Vector3 r3 = (i + 1 == last) ? (2.0f * r2 - r1) : keyframes_[i + 2].rotate;
				transform_.rotate = CatmullRomInterpolation(r0, r1, r2, r3, easedT);

				const Vector3 f1{ kStart.fov, 0.0f, 0.0f };
				const Vector3 f2{ kEnd.fov,   0.0f, 0.0f };
				const Vector3 f0 = (i == 0)        ? (2.0f * f1 - f2)
				                                   : Vector3{ keyframes_[i - 1].fov, 0.0f, 0.0f };
				const Vector3 f3 = (i + 1 == last) ? (2.0f * f2 - f1)
				                                   : Vector3{ keyframes_[i + 2].fov, 0.0f, 0.0f };
				fovY_ = CatmullRomInterpolation(f0, f1, f2, f3, easedT).x;
			}
			else {
				transform_.translate = Lerp(kStart.translate, kEnd.translate, easedT);
				transform_.rotate    = Lerp(kStart.rotate,    kEnd.rotate,    easedT);
				fovY_                = Lerp(kStart.fov,       kEnd.fov,       easedT);
			}
			return;
		}
	}
}

// ============================================================
// 3D デバッグ描画 (パスと球マーカー)
// ============================================================
void KeyframeCamera::DrawDebug3D(Line& line) {
	if (!showPath_ || keyframes_.size() < 2) return;

	// ----------------------------------------
	// パス描画: 隣接 2 キーの間を pathSegmentSamples_ 分割でサンプリング
	//          線を引いて Catmull-Rom 曲線を可視化
	// ----------------------------------------
	auto sampleAt = [&](float globalTime) -> Vector3 {
		// セグメント検索 + 補間（EvaluateAt と同じロジックの位置だけ版）
		float t0 = keyframes_.front().time;
		float t1 = keyframes_.back().time;
		float clampedTime = std::clamp(globalTime, t0, t1);

		for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
			auto& kStart = keyframes_[i];
			auto& kEnd   = keyframes_[i + 1];
			if (clampedTime >= kStart.time && clampedTime <= kEnd.time) {
				float segSpan = std::max(kEnd.time - kStart.time, 1e-6f);
				float t = (clampedTime - kStart.time) / segSpan;
				float easedT = Easing::Ease(kStart.easing, t);

				if (interpolationMode_ == InterpolationMode::CatmullRom
					&& keyframes_.size() >= 3) {
					const size_t last = keyframes_.size() - 1;
					const Vector3& p1 = kStart.translate;
					const Vector3& p2 = kEnd.translate;
					const Vector3 p0 = (i == 0)        ? (2.0f * p1 - p2) : keyframes_[i - 1].translate;
					const Vector3 p3 = (i + 1 == last) ? (2.0f * p2 - p1) : keyframes_[i + 2].translate;
					return CatmullRomInterpolation(p0, p1, p2, p3, easedT);
				}
				return Lerp(kStart.translate, kEnd.translate, easedT);
			}
		}
		return keyframes_.front().translate;
	};

	// Line は SetColor → 登録 → DrawLine() でフラッシュ、の繰り返しで
	// 色別バッチを描画する。色が変わるたびにフラッシュが必要。

	// ----------------------------------------
	// (1) パス線: 黄色
	// ----------------------------------------
	line.SetColor(Vector4{ 0.9f, 0.7f, 0.2f, 1.0f });
	const int samples = std::max(2, pathSegmentSamples_);
	for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
		float t0 = keyframes_[i].time;
		float t1 = keyframes_[i + 1].time;
		Vector3 prev = sampleAt(t0);
		for (int s = 1; s <= samples; ++s) {
			float u = static_cast<float>(s) / samples;
			float t = t0 + (t1 - t0) * u;
			Vector3 cur = sampleAt(t);
			line.RegisterLine(prev, cur);
			prev = cur;
		}
	}
	line.DrawLine();

	// ----------------------------------------
	// (2) 非選択キー: 薄水色
	// ----------------------------------------
	line.SetColor(Vector4{ 0.4f, 0.8f, 1.0f, 1.0f });
	bool anyUnselected = false;
	for (int i = 0; i < static_cast<int>(keyframes_.size()); ++i) {
		if (i == selectedKeyIndex_) continue;
		line.DrawSphere(keyframes_[i].translate, 0.4f, 10);
		anyUnselected = true;
	}
	if (anyUnselected) line.DrawLine();

	// ----------------------------------------
	// (3) 選択中キー: 赤・大きめ
	// ----------------------------------------
	if (selectedKeyIndex_ >= 0
		&& selectedKeyIndex_ < static_cast<int>(keyframes_.size())) {
		line.SetColor(Vector4{ 1.0f, 0.2f, 0.2f, 1.0f });
		line.DrawSphere(keyframes_[selectedKeyIndex_].translate, 0.7f, 12);
		line.DrawLine();
	}

	// ----------------------------------------
	// (4) 現在再生位置 (timer_): 緑
	// ----------------------------------------
	line.SetColor(Vector4{ 0.2f, 1.0f, 0.4f, 1.0f });
	line.DrawSphere(transform_.translate, 0.5f, 12);
	line.DrawLine();
}

// ============================================================
// エディタ用GUI描画
// ============================================================
void KeyframeCamera::DrawDebugGui() {
#ifdef USE_IMGUI
	ImGui::Text("--- キーフレームアニメーション ---");

	// ------------------------------------------------------------
	// 編集モード切替（UE Sequencer 風: プレビュー / 自由視点）
	// ------------------------------------------------------------
	{
		auto director = CameraDirector::GetInstance();
		ImGui::TextDisabled("== 編集モード ==");

		// MainDebug カメラを取得して DebugCamera にキャスト
		auto dbgVCam = director->GetCamera("MainDebug");
		auto dbgCam  = std::dynamic_pointer_cast<DebugCamera>(dbgVCam);

		if (ImGui::Button("Preview (このカメラで見る)")) {
			director->SetPriority(name_, 1000);
			director->SetPriority("MainDebug", 0);
			if (dbgCam) dbgCam->SetEnabled(false);
		}
		ImGui::SameLine();
		if (ImGui::Button("Fly (自由視点)")) {
			director->SetPriority(name_, 0);
			director->SetPriority("MainDebug", 1000);
			if (dbgCam) dbgCam->SetEnabled(true);
		}
		ImGui::TextDisabled("  Fly で構図決め → 「現在の視点で上書き」でキー反映");
		ImGui::TextDisabled("  Fly: WASD 移動 / Q E 上下 / 右ドラッグで回転");
		if (dbgCam) {
			bool enabled = dbgCam->IsEnabled();
			if (ImGui::Checkbox("DebugCamera 入力有効", &enabled)) {
				dbgCam->SetEnabled(enabled);
			}
		}
	}

	ImGui::Checkbox("3D パスを描画", &showPath_);

	ImGui::Separator();

	// ------------------------------------------------------------
	// 再生コントロール
	// ------------------------------------------------------------
	if (ImGui::Button(isPlaying_ ? "一時停止" : "再生")) isPlaying_ = !isPlaying_;
	ImGui::SameLine();
	if (ImGui::Button("リセット")) timer_ = 0.0f;

	// スクラブ: スライダ動かすと即時 EvaluateAt で描画反映
	if (ImGui::SliderFloat("再生時間", &timer_, 0.0f,
		keyframes_.empty() ? 0.0f : keyframes_.back().time)) {
		EvaluateAt(timer_);
	}
	ImGui::Checkbox("ループ再生", &isLooping_);
	ImGui::DragFloat("再生速度", &playbackSpeed_, 0.1f, 0.0f, 5.0f);

	// 補間モード切替
	{
		const char* modeNames[] = { "Linear (直線)", "CatmullRom (曲線)" };
		int modeIdx = static_cast<int>(interpolationMode_);
		if (ImGui::Combo("補間モード", &modeIdx, modeNames, IM_ARRAYSIZE(modeNames))) {
			interpolationMode_ = static_cast<InterpolationMode>(modeIdx);
		}
		ImGui::TextDisabled("  CatmullRom はキーを滑らかに通る曲線（推奨）");
	}

	ImGui::Separator();

	// ------------------------------------------------------------
	// キーフレームの追加
	// ------------------------------------------------------------
	if (ImGui::Button("現在のアングルをキーとして追加")) {
		auto director = CameraDirector::GetInstance();
		float nextTime = keyframes_.empty() ? 0.0f : keyframes_.back().time + 2.0f;

		AddKeyframe(nextTime, director->GetActiveCameraPos(),
			director->GetActiveCameraRot(), director->GetFovY(),
			Easing::Function::EaseInOutQuad);
	}

	// 全 Easing の選択肢（enum 並び順と一致させる）
	static const char* kEasingNames[] = {
		"Linear",
		"EaseInSine", "EaseOutSine", "EaseInOutSine",
		"EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
		"EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
		"EaseInQuart", "EaseOutQuart", "EaseInOutQuart",
		"EaseInQuint", "EaseOutQuint", "EaseInOutQuint",
		"EaseInExpo", "EaseOutExpo", "EaseInOutExpo",
		"EaseInCirc", "EaseOutCirc", "EaseInOutCirc",
		"EaseInBack", "EaseOutBack", "EaseInOutBack",
		"EaseInElastic", "EaseOutElastic", "EaseInOutElastic",
		"EaseInBounce", "EaseOutBounce", "EaseInOutBounce",
		"EaseOutGrowBounce"
	};

	// ------------------------------------------------------------
	// キーフレーム一覧（編集 + 削除 + 上書き + 選択）
	// ------------------------------------------------------------
	bool needSort = false;
	int  removeIndex = -1;

	ImGui::Text("選択中: %s", (selectedKeyIndex_ >= 0)
		? ("Key " + std::to_string(selectedKeyIndex_)).c_str() : "なし");
	if (ImGui::Button("選択解除")) selectedKeyIndex_ = -1;

	if (ImGui::TreeNode("キーフレーム一覧")) {
		for (int i = 0; i < static_cast<int>(keyframes_.size()); ++i) {
			ImGui::PushID(i);
			auto& kf = keyframes_[i];

			// 選択中の Header を強調
			bool isSel = (i == selectedKeyIndex_);
			if (isSel) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.2f, 0.2f, 0.5f));

			if (ImGui::TreeNode("Header", "%sKey %d  (t=%.2f)", isSel ? "★ " : "  ", i, kf.time)) {
				if (ImGui::Button(isSel ? "選択解除##sel" : "このキーを選択")) {
					selectedKeyIndex_ = isSel ? -1 : i;
					timer_ = kf.time;
					EvaluateAt(timer_);
				}

				if (ImGui::DragFloat("時間", &kf.time, 0.05f, 0.0f, 600.0f)) {}
				if (ImGui::IsItemDeactivatedAfterEdit()) needSort = true;

				ImGui::DragFloat3("位置", &kf.translate.x, 0.1f);
				ImGui::DragFloat3("回転 (rad)", &kf.rotate.x, 0.01f);
				ImGui::DragFloat("FOV", &kf.fov, 0.5f, 10.0f, 120.0f);

				int easingIdx = static_cast<int>(kf.easing);
				if (ImGui::Combo("Easing", &easingIdx,
					kEasingNames, IM_ARRAYSIZE(kEasingNames))) {
					kf.easing = static_cast<Easing::Function>(easingIdx);
				}

				if (ImGui::Button("現在の視点で上書き")) {
					auto director = CameraDirector::GetInstance();
					kf.translate = director->GetActiveCameraPos();
					kf.rotate    = director->GetActiveCameraRot();
					kf.fov       = director->GetFovY();
				}
				ImGui::SameLine();
				if (ImGui::Button("ジャンプ")) {
					// このキーの時刻に再生位置をスナップ（プレビュー用）
					timer_ = kf.time;
					EvaluateAt(timer_);
				}
				ImGui::SameLine();
				if (ImGui::Button("削除")) {
					removeIndex = i;
				}

				ImGui::TreePop();
			}
			if (isSel) ImGui::PopStyleColor();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	if (removeIndex >= 0) {
		keyframes_.erase(keyframes_.begin() + removeIndex);
		if (selectedKeyIndex_ == removeIndex) selectedKeyIndex_ = -1;
		else if (selectedKeyIndex_ > removeIndex) --selectedKeyIndex_;
	}
	if (needSort) {
		SortKeyframes();
		selectedKeyIndex_ = -1; // ソートでインデックスが変わるので無効化
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
	j["interpolationMode"] = static_cast<int>(interpolationMode_);

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
	interpolationMode_ = static_cast<InterpolationMode>(
		j.value("interpolationMode", static_cast<int>(InterpolationMode::CatmullRom)));

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