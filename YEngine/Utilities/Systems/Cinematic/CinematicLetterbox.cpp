#include "CinematicLetterbox.h"
#include "Sprite/Sprite.h"
#include "WinApp/WinApp.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Vector2.h"
#include <algorithm>

namespace YoRigine {

	CinematicLetterbox::CinematicLetterbox() = default;
	CinematicLetterbox::~CinematicLetterbox() = default;

	void CinematicLetterbox::Initialize()
	{
		// 真っ黒の Sprite × 2 を作る（white.png を tint）
		topBar_ = std::make_unique<Sprite>();
		topBar_->Initialize("Resources/Textures/white.png");
		topBar_->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
		topBar_->SetAnchorPoint(Vector2{ 0.0f, 0.0f });

		bottomBar_ = std::make_unique<Sprite>();
		bottomBar_->Initialize("Resources/Textures/white.png");
		bottomBar_->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 1.0f });
		bottomBar_->SetAnchorPoint(Vector2{ 0.0f, 0.0f });

		SetProgress(0.0f);
	}

	void CinematicLetterbox::Update()
	{
		if (topBar_)    topBar_->Update();
		if (bottomBar_) bottomBar_->Update();
	}

	void CinematicLetterbox::Draw()
	{
		// progress=0 のときは描画しない（完全に画面外なので無駄）
		if (progress_ <= 0.0f) return;

		if (topBar_)    topBar_->Draw();
		if (bottomBar_) bottomBar_->Draw();
	}

	void CinematicLetterbox::SetProgress(float progress)
	{
		progress_ = std::clamp(progress, 0.0f, 1.0f);

		const float screenW = static_cast<float>(WinApp::kClientWidth);
		const float screenH = static_cast<float>(WinApp::kClientHeight);
		const float barH    = screenH * barHeightRatio_;

		// 上下の Sprite サイズを画面幅×黒帯高に設定
		if (topBar_) {
			topBar_->SetSize(Vector2{ screenW, barH });
			// progress 0: y = -barH (画面外)
			// progress 1: y = 0     (画面上端)
			float y = -barH + barH * progress_;
			topBar_->SetTranslate(Vector3{ 0.0f, y, 0.0f });
		}
		if (bottomBar_) {
			bottomBar_->SetSize(Vector2{ screenW, barH });
			// progress 0: y = screenH        (画面外)
			// progress 1: y = screenH - barH (画面下端)
			float y = screenH - barH * progress_;
			bottomBar_->SetTranslate(Vector3{ 0.0f, y, 0.0f });
		}
	}

}
