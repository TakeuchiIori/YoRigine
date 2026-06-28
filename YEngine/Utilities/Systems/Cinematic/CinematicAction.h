#pragma once
#include <functional>
#include <string>
#include "Easing.h"

namespace YoRigine {

	// =====================================================================
	// ICinematicAction: 演出 1 単位の共通インターフェース
	//   - startTime / endTime はシーケンス全体時間内の発動時刻
	//   - OnStart は startTime 到達時に 1 回
	//   - OnUpdate は localT (0..1) 引数で範囲内毎フレーム
	//   - OnFinish は endTime 経過時に 1 回
	// =====================================================================
	class ICinematicAction {
	public:
		virtual ~ICinematicAction() = default;
		virtual void OnStart() {}
		virtual void OnUpdate(float /*localT*/) {}
		virtual void OnFinish() {}

		float GetStart() const { return startTime_; }
		float GetEnd()   const { return endTime_; }

	protected:
		ICinematicAction(float startTime, float endTime)
			: startTime_(startTime), endTime_(endTime) {}

		float startTime_;
		float endTime_;
	};

	// =====================================================================
	// TweenFloatAction: 任意の float を補間する
	//   - getter は OnStart 時に評価しない（呼び出し側で from 指定する想定）
	//   - setter は毎フレーム書き込む
	// =====================================================================
	class TweenFloatAction : public ICinematicAction {
	public:
		using Setter = std::function<void(float)>;

		TweenFloatAction(Setter setter, float from, float to,
			float startTime, float endTime,
			Easing::Function easing = Easing::Function::Linear)
			: ICinematicAction(startTime, endTime),
			setter_(std::move(setter)), from_(from), to_(to), easing_(easing) {}

		void OnUpdate(float localT) override {
			float eased = Easing::Ease(easing_, localT);
			setter_(from_ + (to_ - from_) * eased);
		}
		void OnFinish() override {
			setter_(to_);
		}

	private:
		Setter setter_;
		float from_;
		float to_;
		Easing::Function easing_;
	};

	// =====================================================================
	// LetterboxAction: 上下黒帯のスライドイン/アウト
	//   show=true で 0→1 へ、show=false で 1→0 へ
	// =====================================================================
	class LetterboxAction : public ICinematicAction {
	public:
		LetterboxAction(bool show, float startTime, float endTime,
			Easing::Function easing = Easing::Function::EaseOutCubic)
			: ICinematicAction(startTime, endTime),
			show_(show), easing_(easing) {}

		void OnUpdate(float localT) override;
		void OnFinish() override;

	private:
		bool show_;
		Easing::Function easing_;
	};

	// =====================================================================
	// CallbackAction: 指定時刻に lambda を 1 回発火（SE 再生・エフェクト起動など）
	// =====================================================================
	class CallbackAction : public ICinematicAction {
	public:
		CallbackAction(float time, std::function<void()> callback)
			: ICinematicAction(time, time),
			callback_(std::move(callback)) {}

		void OnStart() override {
			if (callback_) callback_();
		}

	private:
		std::function<void()> callback_;
	};

	// =====================================================================
	// CameraTrackAction: 指定名の KeyframeCamera を演出中アクティブにする
	//   - OnStart  : cameraName を高優先度に切り替え + Reset + Play
	//   - OnFinish : restoreCameraName を高優先度に戻す + Stop
	//   ※ JSON で事前に登録された KeyframeCamera を再生する設計
	// =====================================================================
	class CameraTrackAction : public ICinematicAction {
	public:
		CameraTrackAction(const std::string& cameraName,
			const std::string& restoreCameraName,
			float startTime, float endTime,
			int activePriority = 1000,
			int restorePriority = 10)
			: ICinematicAction(startTime, endTime),
			cameraName_(cameraName),
			restoreCameraName_(restoreCameraName),
			activePriority_(activePriority),
			restorePriority_(restorePriority) {}

		void OnStart() override;
		void OnFinish() override;

	private:
		std::string cameraName_;
		std::string restoreCameraName_;
		int activePriority_;
		int restorePriority_;
	};

}
