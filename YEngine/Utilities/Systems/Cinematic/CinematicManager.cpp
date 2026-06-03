#include "CinematicManager.h"
#include "CinematicLetterbox.h"
#include "CinematicSequencer.h"

namespace YoRigine {

	CinematicManager::~CinematicManager() = default;

	CinematicManager* CinematicManager::GetInstance()
	{
		static CinematicManager instance;
		return &instance;
	}

	void CinematicManager::Initialize()
	{
		letterbox_ = std::make_unique<CinematicLetterbox>();
		letterbox_->Initialize();
		activeSeq_.reset();
	}

	void CinematicManager::Play(std::unique_ptr<CinematicSequencer> seq)
	{
		activeSeq_ = std::move(seq);
	}

	void CinematicManager::Stop()
	{
		activeSeq_.reset();
		if (letterbox_) letterbox_->SetProgress(0.0f);
	}

	void CinematicManager::Update(float deltaTime)
	{
		if (activeSeq_) {
			activeSeq_->Update(deltaTime);
			// 終了したら破棄
			if (activeSeq_->IsFinished()) {
				activeSeq_.reset();
			}
		}

		if (letterbox_) letterbox_->Update();
	}

	void CinematicManager::Draw()
	{
		if (letterbox_) letterbox_->Draw();
	}

}
