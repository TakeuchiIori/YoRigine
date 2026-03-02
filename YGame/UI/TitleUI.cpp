#include "TitleUI.h"
#include "Systems/GameTime/GameTime.h"
#include <Systems/Input/Input.h>

/// <summary>
/// 初期化処理
/// </summary>
void TitleUI::Initialize()
{
	YoRigine::UIManager::GetInstance()->LoadScene("Title");

	// Aボタン表示（コントローラー接続時）
	startUiController_ = YoRigine::UIManager::GetInstance()->GetUI("A");

	// Space表示（未接続時）
	startUiKeyboard_ = YoRigine::UIManager::GetInstance()->GetUI("Space");

	alpha_ = maxAlpha_;
	isFadingOut_ = true;

	// 初期状態でどちらを表示するか
	bool connected = YoRigine::Input::IsControllerConnected();
	if (startUiController_ && startUiKeyboard_) {
		startUiController_->SetVisible(connected);
		startUiKeyboard_->SetVisible(!connected);
	}
	lastControllerConnected_ = connected;
}

/// <summary>
/// 更新処理
/// </summary>
void TitleUI::Update()
{
	bool connected = YoRigine::Input::IsControllerConnected();

	// 接続状態が変化したらスプライト切替
	if (connected != lastControllerConnected_) {
		if (startUiController_ && startUiKeyboard_) {
			startUiController_->SetVisible(connected);
			startUiKeyboard_->SetVisible(!connected);
		}
		lastControllerConnected_ = connected;
	}

	// 現在表示しているスプライトをフェード処理
	UIBase* activeUi = connected ? startUiController_ : startUiKeyboard_;
	if (!activeUi) return;

	float deltaTime = YoRigine::GameTime::GetDeltaTime();

	if (isFadingOut_) {
		alpha_ -= fadeSpeed_ * deltaTime;
		if (alpha_ <= minAlpha_) {
			alpha_ = minAlpha_;
			isFadingOut_ = false;
		}
	} else {
		alpha_ += fadeSpeed_ * deltaTime;
		if (alpha_ >= maxAlpha_) {
			alpha_ = maxAlpha_;
			isFadingOut_ = true;
		}
	}

	// アルファ値を適用
	Vector4 currentColor = activeUi->GetColor();
	currentColor.w = alpha_;
	activeUi->SetColor(currentColor);

	YoRigine::UIManager::GetInstance()->UpdateAll();
}

/// <summary>
/// 描画処理
/// </summary>
void TitleUI::Draw()
{
	YoRigine::UIManager::GetInstance()->DrawAll();
}
