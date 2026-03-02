#include "PlayerHealthBarUI.h"
#include "Player/Player.h"
#include <Systems/UI/UIManager.h>

PlayerHealthBarUI::PlayerHealthBarUI(const Player* player)
{
    player_ = player;
}

void PlayerHealthBarUI::Initialize()
{
    bgHP_ = YoRigine::UIManager::GetInstance()->GetUI("playerHPBG");
    barHP_ = YoRigine::UIManager::GetInstance()->GetUI("playerHPBar");
    size_ = barHP_->GetScale();
}

void PlayerHealthBarUI::Update()
{
    // HP割合
    float ratio = (float)player_->GetHP() / (float)player_->GetMaxHP();
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    currentRatio_ = ratio;
    barHP_->SetScale({ size_.x * ratio, size_.y});

    bgHP_->Update();
    barHP_->Update();
}
