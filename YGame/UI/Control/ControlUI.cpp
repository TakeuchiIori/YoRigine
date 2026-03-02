#include "ControlUI.h"
#include <Systems/Input/Input.h>
#include <Systems/GameTime/GameTime.h>

void ControlUI::Initialize()
{
	// コントロールUI取得
	button_[0] = YoRigine::UIManager::GetInstance()->GetUI("A_attack");
	button_[1] = YoRigine::UIManager::GetInstance()->GetUI("B_attack");
	button_[2] = YoRigine::UIManager::GetInstance()->GetUI("shield");

	// 波紋用のUI
	ripples = YoRigine::UIManager::GetInstance()->GetUI("ripples");
    ripples->SetVisible(false);
    originalSize_ = {100.0f,100.0f};
}

void ControlUI::Update()
{
    if (YoRigine::GameTime::IsPause()) {
        isVisble_ = false;
    }
    else {
	   isVisble_ = true;
    }

    auto layer = YoRigine::UIManager::GetInstance()->GetUIsByLayer(1);
    for (auto& ui : layer) {
        ui->SetVisible(isVisble_);
    }

    // Aボタンが押された瞬間
    if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::A)) {
        if (!isVisble_)return;
        TriggerRipple(button_[0]);
		button_[0]->PlayScaleAnimation(pushSize_, originalSize_, duration_, Easing::Function::EaseInCubic, false);
        button_[0]->PlayFlash(duration_, 9);
    }

    // Bボタンが押された瞬間
    if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::B)) {
        if (!isVisble_)return;
        TriggerRipple(button_[1]);
        button_[1]->PlayScaleAnimation(pushSize_, originalSize_, duration_, Easing::Function::EaseInCubic, false);
        button_[1]->PlayFlash(duration_, 9);
    }

    // Xボタンが押された瞬間
    if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::X)) {
        if (!isVisble_)return;
        TriggerRipple(button_[2]);
        button_[2]->PlayScaleAnimation({ 100.0f ,100.0f }, Vector2{120.0f,120.0f}, duration_, Easing::Function::EaseInCubic, false);
        button_[2]->PlayFlash(duration_, 9);
    }


    // 波紋のアニメーションが終わったら非表示にする
    if (ripples && ripples->IsVisible() && !ripples->IsAnimating()) {
        ripples->SetVisible(false);
    }

    UpdateRipples();
}

void ControlUI::Draw()
{
    auto layer = YoRigine::UIManager::GetInstance()->GetUIsByLayer(1);
    for (auto& ui : layer) {
        ui->Draw();
    }
    // 波紋の描画
    for (auto& ripple : activeRipples_) {
        ripple->Draw();
    }
}

// 波紋の生存管理
void ControlUI::UpdateRipples()
{
    for (auto it = activeRipples_.begin(); it != activeRipples_.end(); ) {
        (*it)->Update();

        // アニメーションが終了したら削除
        if (!(*it)->IsAnimating()) {
            it = activeRipples_.erase(it);
        }
        else {
            ++it;
        }
    }
}

// 波紋を発生させる
void ControlUI::TriggerRipple(UIBase* targetButton)
{
    if (!ripples || !targetButton || !isVisble_) return;

    // プロトタイプから新しい波紋インスタンスを作成（クローン）
    auto newRipple = std::make_unique<UIBase>();
    newRipple->Initialize("");
    newRipple->CopyPropertiesFrom(ripples);

    // ボタンの位置に合わせる
    newRipple->SetPosition(targetButton->GetPosition());

    // 初期状態リセット（見えない状態から開始）
    newRipple->SetVisible(true);


    // アニメーション設定：拡大しながらフェードアウト
    newRipple->PlayScaleAnimation({ 120.0f, 120.0f }, { 200.0f, 200.0f }, duration_, Easing::Function::EaseInCubic, false);
    newRipple->PlayAlphaAnimation(1.0f, 0.0f, duration_, Easing::Function::Linear, false);

    // 管理リストに追加
    activeRipples_.push_back(std::move(newRipple));
}
