#include "CinematicAction.h"
#include "CinematicManager.h"
#include "CinematicLetterbox.h"
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/Virtuals/KeyframeCamera/KeyframeCamera.h"

namespace YoRigine {

	void LetterboxAction::OnUpdate(float localT)
	{
		auto* mgr = CinematicManager::GetInstance();
		auto* lb = mgr ? mgr->GetLetterbox() : nullptr;
		if (!lb) return;

		float eased = Easing::Ease(easing_, localT);
		float target = show_ ? eased : (1.0f - eased);
		lb->SetProgress(target);
	}

	void LetterboxAction::OnFinish()
	{
		auto* mgr = CinematicManager::GetInstance();
		auto* lb = mgr ? mgr->GetLetterbox() : nullptr;
		if (!lb) return;

		// 確実に終端値にスナップ
		lb->SetProgress(show_ ? 1.0f : 0.0f);
	}

	// -----------------------------------------------------------------
	// CameraTrackAction
	// -----------------------------------------------------------------
	void CameraTrackAction::OnStart()
	{
		auto director = CameraDirector::GetInstance();
		if (!director) return;

		// 指定カメラを取り出し
		auto cam = director->GetCamera(cameraName_);
		if (!cam) return;

		// KeyframeCamera なら Reset + Play
		if (auto kf = std::dynamic_pointer_cast<KeyframeCamera>(cam)) {
			kf->Reset();
			kf->SetLooping(false); // 演出中はループしない
			kf->Play();
		}

		// 優先度を上げてアクティブカメラに
		director->SetPriority(cameraName_, activePriority_);
	}

	void CameraTrackAction::OnFinish()
	{
		auto director = CameraDirector::GetInstance();
		if (!director) return;

		auto cur = director->GetCamera(cameraName_);
		if (cur) {
			// KeyframeCamera なら再生停止（最後の評価位置で固定）
			if (auto kf = std::dynamic_pointer_cast<KeyframeCamera>(cur)) {
				kf->Stop();
			}
			// 復帰先指定がある場合のみ priority を下げて切替を発生させる。
			// 指定が無い場合は cinematic camera を active のまま維持し、
			// シーン遷移などで自然に解消することを期待する
			// （priority を下げると PlayerFollow 等が即座に active になり、
			//   シーン切替のフェード中に背景カメラが見えてしまうため）。
			if (!restoreCameraName_.empty()) {
				director->SetPriority(cameraName_, 0);
				director->SetPriority(restoreCameraName_, restorePriority_);
			}
		}
	}

}
