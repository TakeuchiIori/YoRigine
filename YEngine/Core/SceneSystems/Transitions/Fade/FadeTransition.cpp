#include "FadeTransition.h"

/// <summary>
/// フェードトランジションの初期化
/// </summary>
void FadeTransition::Initialize() {

	//------------------------------------------------------------
	// フェード用インスタンス生成
	//------------------------------------------------------------
	fade_ = std::make_unique<Fade>();
	fade_->Initialize("Resources/images/white.png");
}

/// <summary>
/// 更新処理
/// </summary>
void FadeTransition::Update() {
	fade_->Update();
}

/// <summary>
/// 描画処理
/// </summary>
void FadeTransition::Draw() {
	SpriteCommon::GetInstance()->DrawPreference();
	fade_->Draw();
}

/// <summary>
/// トランジション完了判定
/// </summary>
bool FadeTransition::IsFinished() const {
	return fade_->IsFinished();
}

/// <summary>
/// フェードイン開始（トランジション開始）
/// </summary>
void FadeTransition::StartTransition() {
	fade_->Start(Fade::Status::FadeIn, FADE_DURATION);
}

/// <summary>
/// フェードアウト開始（トランジション終了）
/// </summary>
void FadeTransition::EndTransition() {
	fade_->Start(Fade::Status::FadeOut, FADE_DURATION);
}
