#include "FadeTransition.h"

void FadeTransition::Initialize()
{
	fade_ = std::make_unique<Fade>();
	fade_->Initialize("Resources/images/white.png");
}

void FadeTransition::Update()
{
	fade_->Update();
}

void FadeTransition::Draw()
{
	SpriteCommon::GetInstance()->DrawPreference();
	fade_->Draw();
}

bool FadeTransition::IsFinished() const
{
	return fade_->IsFinished();
}

void FadeTransition::StartTransition()
{
	fade_->Start(Fade::Status::FadeIn, FADE_DURATION);
}

void FadeTransition::EndTransition()
{
	fade_->Start(Fade::Status::FadeOut, FADE_DURATION);
}
