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
        // 絶対px指定を基準サイズ基準の倍率に変換（表示は従来どおり）
        Vector2 baseA = button_[0]->GetSize();
        button_[0]->PlayScaleAnimation(pushSize_ / baseA, originalSize_ / baseA, duration_, Easing::Function::EaseInCubic, false);
        button_[0]->PlayFlash(duration_, 9);
    }

    // Bボタンが押された瞬間
    if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::B)) {
        if (!isVisble_)return;
        TriggerRipple(button_[1]);
        Vector2 baseB = button_[1]->GetSize();
        button_[1]->PlayScaleAnimation(pushSize_ / baseB, originalSize_ / baseB, duration_, Easing::Function::EaseInCubic, false);
        button_[1]->PlayFlash(duration_, 9);
    }

    // Xボタンが押された瞬間
    if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::X)) {
        if (!isVisble_)return;
        TriggerRipple(button_[2]);
        Vector2 baseX = button_[2]->GetSize();
        button_[2]->PlayScaleAnimation(Vector2{ 100.0f,100.0f } / baseX, Vector2{ 120.0f,120.0f } / baseX, duration_, Easing::Function::EaseInCubic, false);
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
    // 絶対px指定を基準サイズ基準の倍率に変換（表示は従来どおり）
    Vector2 baseRipple = newRipple->GetSize();
    newRipple->PlayScaleAnimation(Vector2{ 120.0f, 120.0f } / baseRipple, Vector2{ 200.0f, 200.0f } / baseRipple, duration_, Easing::Function::EaseInCubic, false);
    newRipple->PlayAlphaAnimation(1.0f, 0.0f, duration_, Easing::Function::Linear, false);

    // 管理リストに追加
    activeRipples_.push_back(std::move(newRipple));
}
