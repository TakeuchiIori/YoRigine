#pragma once
#include "Systems/UI/UIBase.h"
#include "Loaders/Json/JsonManager.h"

class Player;
class PlayerHealthBarUI
{
public:
    // コンストラクタ
    PlayerHealthBarUI(const Player* player);

    // 初期化 (UIBase::Initializeをオーバーロード)
    void Initialize();

    // 更新処理 (ビルボード処理と表示ポリシーの適用)
    void Update();
private:
    const Player* player_ = nullptr;

    // HPバーの背景とゲージ用
    UIBase* bgHP_ = nullptr;
    UIBase* barHP_ = nullptr;

    // HPの割合
    float currentRatio_ = 1.0f;

    Vector2 size_{};
};

