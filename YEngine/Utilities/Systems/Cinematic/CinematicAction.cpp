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

		// 元のカメラに戻す
		auto cur = director->GetCamera(cameraName_);
		if (cur) {
			if (auto kf = std::dynamic_pointer_cast<KeyframeCamera>(cur)) {
				kf->Stop();
			}
			// アクティブを降格
			director->SetPriority(cameraName_, 0);
		}

		// 戻り先カメラに高優先度を再付与（指定があれば）
		if (!restoreCameraName_.empty()) {
			director->SetPriority(restoreCameraName_, restorePriority_);
		}
	}

}
