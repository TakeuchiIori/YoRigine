#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "CinematicAction.h"

namespace YoRigine {

	// =====================================================================
	// CinematicSequencer: 演出 1 本のタイムライン
	//   - Duration を持ち、Action を並列に積み上げる
	//   - Update(dt) で進行、終了時に OnFinish コールバックを発火
	//   - CinematicManager 経由で Play する想定
	// =====================================================================
	class CinematicSequencer {
	public:
		explicit CinematicSequencer(float duration) : duration_(duration) {}

		// 任意の Action を直接追加
		void AddAction(std::unique_ptr<ICinematicAction> action);

		// ----- 利便メソッド (内部で Action を作って AddAction) -----
		void Tween(TweenFloatAction::Setter setter,
			float from, float to,
			float startTime, float endTime,
			Easing::Function easing = Easing::Function::Linear);

		void Letterbox(bool show, float startTime, float endTime,
			Easing::Function easing = Easing::Function::EaseOutCubic);

		void Callback(float time, std::function<void()> cb);

		// 指定名 KeyframeCamera を演出時間中アクティブにし、終了時に
		// restoreCameraName のカメラへ戻す（restorePriority で復帰）
		void Camera(const std::string& cameraName,
			const std::string& restoreCameraName,
			float startTime, float endTime,
			int activePriority = 1000,
			int restorePriority = 10);

		// 終了時コールバック
		void OnFinish(std::function<void()> cb) { onFinish_ = std::move(cb); }

		// シーケンス時間進行
		void Update(float dt);

		bool IsFinished() const { return finished_; }
		float GetCurrentTime() const { return currentTime_; }
		float GetDuration()    const { return duration_; }

	private:
		struct ActionEntry {
			std::unique_ptr<ICinematicAction> action;
			bool started  = false;
			bool finished = false;
		};

		float duration_;
		float currentTime_ = 0.0f;
		bool  finished_    = false;

		std::vector<ActionEntry> entries_;
		std::function<void()> onFinish_;
	};

}
