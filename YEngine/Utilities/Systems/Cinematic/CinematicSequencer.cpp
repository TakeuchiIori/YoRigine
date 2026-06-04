#include "CinematicSequencer.h"
#include <algorithm>

namespace YoRigine {

	void CinematicSequencer::AddAction(std::unique_ptr<ICinematicAction> action)
	{
		if (!action) return;
		ActionEntry entry;
		entry.action = std::move(action);
		entries_.push_back(std::move(entry));
	}

	void CinematicSequencer::Tween(TweenFloatAction::Setter setter,
		float from, float to,
		float startTime, float endTime,
		Easing::Function easing)
	{
		AddAction(std::make_unique<TweenFloatAction>(
			std::move(setter), from, to, startTime, endTime, easing));
	}

	void CinematicSequencer::Letterbox(bool show, float startTime, float endTime,
		Easing::Function easing)
	{
		AddAction(std::make_unique<LetterboxAction>(show, startTime, endTime, easing));
	}

	void CinematicSequencer::Callback(float time, std::function<void()> cb)
	{
		AddAction(std::make_unique<CallbackAction>(time, std::move(cb)));
	}

	void CinematicSequencer::Camera(const std::string& cameraName,
		const std::string& restoreCameraName,
		float startTime, float endTime,
		int activePriority, int restorePriority)
	{
		AddAction(std::make_unique<CameraTrackAction>(
			cameraName, restoreCameraName,
			startTime, endTime,
			activePriority, restorePriority));
	}

	void CinematicSequencer::Camera(const std::string& cameraName,
		float startTime, float endTime,
		int activePriority)
	{
		// restoreCameraName を空文字にすることで CameraTrackAction::OnFinish は復帰処理をスキップ
		AddAction(std::make_unique<CameraTrackAction>(
			cameraName, /*restoreCameraName*/ std::string{},
			startTime, endTime,
			activePriority, /*restorePriority*/ 0));
	}

	void CinematicSequencer::Update(float dt)
	{
		if (finished_) return;

		currentTime_ += dt;

		// 各 Action の状態に応じて Start / Update / Finish を発火
		for (auto& entry : entries_) {
			if (entry.finished) continue;

			float s = entry.action->GetStart();
			float e = entry.action->GetEnd();

			// まだ範囲前なら何もしない
			if (currentTime_ < s) continue;

			// 初回 Start
			if (!entry.started) {
				entry.action->OnStart();
				entry.started = true;
			}

			// Update（localT 0..1）
			if (currentTime_ < e) {
				float span = std::max(e - s, 1e-6f);
				float localT = (currentTime_ - s) / span;
				localT = std::clamp(localT, 0.0f, 1.0f);
				entry.action->OnUpdate(localT);
			}
			else {
				// 終端到達: 最後に localT=1.0 で更新 → Finish
				entry.action->OnUpdate(1.0f);
				entry.action->OnFinish();
				entry.finished = true;
			}
		}

		// シーケンス全体終了判定
		if (currentTime_ >= duration_) {
			finished_ = true;
			// 未完了で開始済みの Action を強制 finalize
			// endTime が duration_ を超えていても、letterbox の SetProgress(0) や
			// CameraTrack の優先度復帰がきちんと走るようにする安全網
			for (auto& entry : entries_) {
				if (entry.started && !entry.finished) {
					entry.action->OnUpdate(1.0f);
					entry.action->OnFinish();
					entry.finished = true;
				}
			}
			if (onFinish_) onFinish_();
		}
	}

}
