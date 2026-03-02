#include "Fade.h"
#include <algorithm>
#include "../Core/WinApp/WinApp.h"

/// <summary>
/// フェードの初期化（黒背景スプライト生成）
/// </summary>
void Fade::Initialize(const std::string textureFilePath) {

	//------------------------------------------------------------
	// フェード用スプライト初期化
	//------------------------------------------------------------
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(textureFilePath);
	sprite_->SetSize(Vector2(WinApp::kClientWidth, WinApp::kClientHeight));
	sprite_->SetTextureSize(Vector2(WinApp::kClientWidth, WinApp::kClientHeight));
	sprite_->SetColor(Vector4(0, 0, 0, 0)); // 初期状態は透明
}

/// <summary>
/// フェード更新処理
/// </summary>
void Fade::Update() {

	//------------------------------------------------------------
	// 状態ごとの処理分岐
	//------------------------------------------------------------
	switch (status_) {
	case Status::None:
		// 無効状態
		break;

	case Status::FadeIn:
		FadeIn();
		break;

	case Status::FadeOut:
		FadeOut();
		break;
	}
}

/// <summary>
/// フェードイン処理（暗 → 明）
/// </summary>
void Fade::FadeIn() {
	counter_ += 1.0f / 60.0f; // 1フレーム進行
	if (counter_ >= duration_) counter_ = duration_;

	float alpha = std::clamp(1.0f - counter_ / duration_, 0.0f, 1.0f);
	sprite_->SetColor(Vector4(0, 0, 0, alpha));
	sprite_->Update();
}

/// <summary>
/// フェードアウト処理（明 → 暗）
/// </summary>
void Fade::FadeOut() {
	counter_ += 1.0f / 60.0f; // 1フレーム進行
	if (counter_ >= duration_) counter_ = duration_;

	float alpha = std::clamp(counter_ / duration_, 0.0f, 1.0f);
	sprite_->SetColor(Vector4(0, 0, 0, alpha));
	sprite_->Update();
}

/// <summary>
/// フェード描画
/// </summary>
void Fade::Draw() {
	if (status_ == Status::None) return;
	sprite_->Draw();
}

/// <summary>
/// フェード開始
/// </summary>
/// <param name="status">フェードイン or フェードアウト</param>
/// <param name="duration">完了までの秒数</param>
void Fade::Start(Status status, float duration) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;
}

/// <summary>
/// フェード停止（状態リセット）
/// </summary>
void Fade::Stop() {
	status_ = Status::None;
}

/// <summary>
/// フェード完了判定
/// </summary>
/// <returns>trueならフェード終了</returns>
bool Fade::IsFinished() const {
	switch (status_) {
	case Fade::Status::FadeIn:
	case Fade::Status::FadeOut:
		return (counter_ >= duration_);
	}
	return true;
}
