#include "Fade.h"
#include "Fade.h"
#include "algorithm"
void Fade::Initialize(const std::string textureFilePath) {
	// スプライトの生成
	sprite_ = make_unique<Sprite>();
	sprite_->Initialize(textureFilePath);
	sprite_->SetSize(Vector2(1280, 720));
	sprite_->SetTextureSize(Vector2(1280, 720));
	sprite_->SetColor(Vector4(0, 0, 0, 1));

}

void Fade::Update() {
	// フェード状態による分岐
	switch (status_) {
	case Status::None:
		// 何もしない

		break;
	case Status::FadeIn:

		FadeIn();

		break;
	case Status::FadeOut:

		FadeOut();

		break;
	}

	
}

void Fade::FadeIn()
{
	// 1フレーム分の秒数をカウントアップ
	counter_ += 1.0f / 60.0f;
	// フェード継続時間に達したら打ち止め
	if (counter_ >= duration_) {
		counter_ = duration_;
	}
	// 1.0fから0.0fの間で、経過時間がフェード継続時間に近づくほどアルファ値を小さくする
	sprite_->SetColor(Vector4{ 0,0,0,std::clamp(1.0f - counter_ / duration_, 0.0f, 1.0f) });
	sprite_->Update();

}

void Fade::FadeOut()
{
	// 1フレーム分の秒数をカウントアップ
	counter_ += 1.0f / 60.0f;
	// フェード継続時間に達したら打ち止め
	if (counter_ >= duration_) {
		counter_ = duration_;
	}
	// 0.0fから1.0fの間で、経過時間がフェード継続時間に近づくほどアルファ値を大きくする
	sprite_->SetColor(Vector4{ 0,0,0,std::clamp(counter_ / duration_, 0.0f, 1.0f) });
	sprite_->Update();
}

void Fade::Draw() {
	if (status_ == Status::None) {
		return;
	}
	sprite_->Draw();
}

void Fade::Start(Status status, float duration) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;

}

void Fade::Stop() {
	status_ = Status::None;

}

bool Fade::IsFinished() const {
	// フェード状態による分岐
	switch (status_) {
	case Fade::Status::FadeIn:
	case Fade::Status::FadeOut:
		if (counter_ >= duration_) {
			return true;
		}
		else {
			return false;
		}
	}

	return true;

}


