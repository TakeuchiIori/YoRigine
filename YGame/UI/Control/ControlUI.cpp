#include "ControlUI.h"
#include <Systems/Input/Input.h>
#include <Systems/GameTime/GameTime.h>
#include <Systems/Text/TextTextureBaker.h>
#include <filesystem>
#include <cmath>

void ControlUI::Initialize()
{
    BakeStyleButtonTextures();

	// コントロールUI取得
	button_[0] = YoRigine::UIManager::GetInstance()->GetUI("A_attack");
	button_[1] = YoRigine::UIManager::GetInstance()->GetUI("B_attack");
	button_[2] = YoRigine::UIManager::GetInstance()->GetUI("shield");
    button_[3] = GetOrCreateButton("Y_style", "Resources/Textures/Operation/Y_to_magic.png", { 1405.0f, 687.0f }, { 122.0f, 122.0f });
    ApplyStyleTextures();

	// 波紋用のUI
	ripples = YoRigine::UIManager::GetInstance()->GetUI("ripples");
    ripples->SetVisible(false);

    // A/B/X ボタンの背面アウトライン枠（攻撃ボタンと一緒に戦闘中のみ表示する）
    attackOutline_[0] = YoRigine::UIManager::GetInstance()->GetUI("outlineUI");
    attackOutline_[1] = YoRigine::UIManager::GetInstance()->GetUI("outlineUI2");
    attackOutline_[2] = YoRigine::UIManager::GetInstance()->GetUI("outlineUI3");
    attackOutline_[3] = YoRigine::UIManager::GetInstance()->GetUI("outlineUI4");

    // A/B/X ボタンの黒背景（同じく攻撃ボタンと一緒に戦闘中のみ表示する）
    buttonBG_[0] = YoRigine::UIManager::GetInstance()->GetUI("buttonBG");
    buttonBG_[1] = YoRigine::UIManager::GetInstance()->GetUI("buttonBG2");
    buttonBG_[2] = YoRigine::UIManager::GetInstance()->GetUI("buttonBG2_1");
    buttonBG_[3] = YoRigine::UIManager::GetInstance()->GetUI("buttonBG2_1_1");
    originalSize_ = {100.0f,100.0f};

    // 角丸四角の枠テクスチャを生成（丸枠が glyph と合わないため）。無ければ作る。
    const std::string frameTex = "Resources/Textures/Operation/frame_square.png";
    if (!std::filesystem::exists(frameTex)) {
        YoRigine::TextTextureBaker::BakeRoundedRectFrame(
            128, 128, /*cornerRadius*/18.0f, /*borderThickness*/8.0f,
            Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, frameTex);
    }

    // LB / RB の操作ヒントを用意（シーンに既にあればそれを使い、無ければ生成）。
    // outline を背面、グリフを前面に。画面(1600x900)の右上寄りに配置（アンカー中心）。
    GetOrCreateButton("ExLB_outline", frameTex, { 1360.0f, 150.0f }, { 120.0f, 120.0f });
    GetOrCreateButton("ExRB_outline", frameTex, { 1480.0f, 150.0f }, { 120.0f, 120.0f });
    lbButton_ = GetOrCreateButton("ExLB", "Resources/Textures/Operation/Ex_LB.png", { 1360.0f, 150.0f }, { 100.0f, 100.0f });
    rbButton_ = GetOrCreateButton("ExRB", "Resources/Textures/Operation/Ex_RB.png", { 1480.0f, 150.0f }, { 100.0f, 100.0f });

    // ロックオン操作ヒント（照準＋右スティック）。テキストは廃止し、記号と動きで伝える。
    // 左上のポーズメニューアイコン(startButton, (10,10)/100px)の下に横並びで配置（90px・調整可）。
    lockOnIcon_  = GetOrCreateButton("LockOnHintIcon",  "Resources/Textures/GameScene/LockOn.png", { 70.0f, 185.0f }, { 90.0f, 90.0f });
    lockOnStick_ = GetOrCreateButton("LockOnHintStick", "Resources/Textures/Operation/Right stick.png", { 170.0f, 185.0f }, { 90.0f, 90.0f });

    // スティック押し込みバウンドの基準位置を控える（実際の上下動は Update で手動適用）。
    if (lockOnStick_) {
        stickBase_ = lockOnStick_->GetPosition();
        stickPrevOffsetY_ = 0.0f;
        stickBaseInit_ = true;
    }

    // 画面外ヒット時に一瞬だけ出す照準（画面中央・大きめ）。普段は非表示で FlashLockOn 時のみ再生。
    lockOnFlash_ = GetOrCreateButton("LockOnFlash", "Resources/Textures/GameScene/LockOn.png", { 800.0f, 450.0f }, { 160.0f, 160.0f });
    if (lockOnFlash_) lockOnFlash_->SetVisible(false);
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

    // ロックオンヒント（照準＋スティック＋ラベル）はバトル中のみ表示する
    const bool lockHintVisible = isVisble_ && battleActive_;
    if (lockOnIcon_)  lockOnIcon_->SetVisible(lockHintVisible);
    if (lockOnStick_) lockOnStick_->SetVisible(lockHintVisible);

    // 攻撃/ガード/スタイル切替のボタンヒント(A/B/X/Y)と、その背面アウトライン枠は戦闘中のみ表示
    // （フィールドでは攻撃できないため）。LB/RB はフィールドでもカメラリセンターとして
    // 機能するので常時表示のまま残す。
    const bool combatHintVisible = isVisble_ && battleActive_;
    for (int i = 0; i < 4; ++i) {
        if (button_[i]) button_[i]->SetVisible(combatHintVisible);
    }
    for (int i = 0; i < 4; ++i) {
        if (attackOutline_[i]) attackOutline_[i]->SetVisible(combatHintVisible);
        if (buttonBG_[i])      buttonBG_[i]->SetVisible(combatHintVisible);
    }

    // フラッシュ照準はアニメ再生中だけ表示（終わったら自動で消える）
    if (lockOnFlash_) {
        lockOnFlash_->SetVisible(isVisble_ && lockOnFlash_->IsAnimating());
    }

    // スティック押し込みバウンド（手動）。エディタで動かされたら基準を取り直して両立させる。
    if (lockOnStick_ && stickBaseInit_) {
        Vector3 cur = lockOnStick_->GetPosition();
        Vector3 inferred = cur; inferred.y -= stickPrevOffsetY_; // 前回オフセットを除いた素の位置
        if (std::abs(inferred.x - stickBase_.x) > 0.01f ||
            std::abs(inferred.y - stickBase_.y) > 0.01f ||
            std::abs(inferred.z - stickBase_.z) > 0.01f) {
            stickBase_ = inferred;  // エディタ等で動かされた → 基準を更新
        }
        constexpr float kBobSpeed = 7.0f;   // 角速度(rad/s)：周期≒0.9s
        constexpr float kBobAmp   = 12.0f;  // 押し込み量(px)
        stickBobPhase_ += YoRigine::GameTime::GetUnscaledDeltaTime() * kBobSpeed;
        float off = (std::sin(stickBobPhase_) * 0.5f + 0.5f) * kBobAmp; // 0..amp（下方向）
        lockOnStick_->SetPosition({ stickBase_.x, stickBase_.y + off, stickBase_.z });
        stickPrevOffsetY_ = off;
    }

    // A/B/X/Y の押下演出（波紋＋ポップ）は戦闘中のみ（フィールドでは攻撃しないため波紋も出さない）
    if (battleActive_) {
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

    // Yボタンが押された瞬間
    if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::Y)) {
        if (!isVisble_)return;
        PlayButtonPress(button_[3]);
    }
    } // if (battleActive_) — A/B/X/Y 押下演出ここまで

    // LB / RB はバトル中のみ押下アニメ（A/B と全く同じ：波紋＋拡縮＋フラッシュ）
    if (battleActive_ && isVisble_) {
        if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::LB)) {
            PlayButtonPress(lbButton_);
        }
        if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::RB)) {
            PlayButtonPress(rbButton_);
        }
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

// 指定IDのUIを取得。無ければ生成して layer1 に登録する
UIBase* ControlUI::GetOrCreateButton(const std::string& id, const std::string& texturePath,
    const Vector2& pos, const Vector2& size)
{
    auto* mgr = YoRigine::UIManager::GetInstance();
    if (UIBase* exist = mgr->GetUI(id)) return exist;  // 既にシーンに用意済みならそれを使う

    // 個別保存（SaveToJSON）と次回起動の復元のため、必ず configPath を持たせる。
    // configPath が空だと UIBase::SaveToJSON は何も書かずに失敗する。
    const std::string cfg = "Resources/UIConfigs/GameScene/" + id + ".json";
    const bool hadCfg = std::filesystem::exists(cfg);

    auto ui = std::make_unique<UIBase>(id);
    ui->Initialize(cfg);   // 既存あれば位置/サイズ/テクスチャを復元、無ければ configPath_ を設定

    if (!hadCfg) {
        // 初回のみデフォルトを適用し、その値を書き出す（次回以降は保存値を尊重）
        ui->SetTexture(texturePath);          // ChangeTexture が実寸を反映（UV/サイズ）
        if (size.x > 0.0f && size.y > 0.0f) ui->SetSize(size); // 0 はテクスチャ実寸を使う
        ui->SetAnchorPoint({ 0.5f, 0.5f });   // 中心基準（拡縮アニメが中心から効く）
        ui->SetLayer(1);
        ui->SetPosition({ pos.x, pos.y, 0.0f });
        ui->SaveToJSON();                     // 正しい初期値を保存（Initialize の白塗りデフォルトを上書き）
    }
    ui->SetVisible(true);

    UIBase* raw = ui.get();
    mgr->AddUI(id, std::move(ui));
    return raw;
}

// ボタン押下時の共通アニメ（波紋＋拡縮ポップ＋フラッシュ）。A/B と全く同じ。
void ControlUI::PlayButtonPress(UIBase* button)
{
    if (!button || !isVisble_) return;
    TriggerRipple(button);
    Vector2 base = button->GetSize();
    button->PlayScaleAnimation(pushSize_ / base, originalSize_ / base, duration_, Easing::Function::EaseInCubic, false);
    button->PlayFlash(duration_, 9);
}

void ControlUI::SetPlayerStyle(PlayerStyle style)
{
    if (currentStyle_ == style) return;
    currentStyle_ = style;
    ApplyStyleTextures();
}

void ControlUI::BakeStyleButtonTextures()
{
    auto bakeLabel = [](const std::string& text, const std::string& path) {
        if (std::filesystem::exists(path)) return;

        YoRigine::TextBakeParams params;
        params.text = text;
        params.fontFilePath = "Resources/Fonts/kurobara-cinderella.ttf";
        params.fontSize = 56.0f;
        params.fillColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        params.outlineWidth = 5.0f;
        params.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        params.padding = 12.0f;
        YoRigine::TextTextureBaker::Bake(params, path);
    };

    bakeLabel("A Magic", "Resources/Textures/Operation/A_magic.png");
    bakeLabel("B Magic", "Resources/Textures/Operation/B_magic.png");
    bakeLabel("X Magic", "Resources/Textures/Operation/X_magic.png");
    bakeLabel("Y Magic", "Resources/Textures/Operation/Y_to_magic.png");
    bakeLabel("Y Sword", "Resources/Textures/Operation/Y_to_sword.png");
}

void ControlUI::ApplyStyleTextures()
{
    if (currentStyle_ == PlayerStyle::Sword) {
        SetButtonTextureKeepLayout(button_[0], "Resources/Textures/Operation/A_attack.png");
        SetButtonTextureKeepLayout(button_[1], "Resources/Textures/Operation/B_attack.png");
        SetButtonTextureKeepLayout(button_[2], "Resources/Textures/Operation/shield.png");
        SetButtonTextureKeepLayout(button_[3], "Resources/Textures/Operation/Y_to_magic.png");
        return;
    }

    SetButtonTextureKeepLayout(button_[0], "Resources/Textures/Operation/A_magic.png");
    SetButtonTextureKeepLayout(button_[1], "Resources/Textures/Operation/B_magic.png");
    SetButtonTextureKeepLayout(button_[2], "Resources/Textures/Operation/X_magic.png");
    SetButtonTextureKeepLayout(button_[3], "Resources/Textures/Operation/Y_to_sword.png");
}

void ControlUI::SetButtonTextureKeepLayout(UIBase* button, const std::string& texturePath)
{
    if (!button) return;

    const Vector2 layoutSize = button->GetSize();

    // Sprite::ChangeTexture はUV範囲を新テクスチャ実寸へ合わせるために size も更新する。
    // ただし操作ボタンの見た目サイズは GameScene の UIConfig が決めるレイアウト情報なので、
    // テクスチャ差し替え後に同じ size を戻し、画像内容だけを切り替える境界にしておく。
    button->SetTexture(texturePath);
    if (layoutSize.x > 0.0f && layoutSize.y > 0.0f) {
        button->SetSize(layoutSize);
    }
}

// 画面外ヒット時のロックオン照準フラッシュ（赤→黄にフェードしながら消える）
void ControlUI::FlashLockOn()
{
    if (!lockOnFlash_) return;

    lockOnFlash_->SetVisible(true);

    const Vector4 red{ 1.0f, 0.0f, 0.0f, 1.0f };
    const Vector4 yellow{ 1.0f, 1.0f, 0.0f, 1.0f };  // 不透明の黄＝ロック確定色
    lockOnFlash_->SetColor(red);

    // ① 色：赤→黄を素早く（ターゲット捕捉 → ロック確定）。アルファはここでは保つ。
    lockOnFlash_->PlayColorAnimation(red, yellow, 0.30f, Easing::Function::EaseOutCubic, false);
    // ② アルファ：しばらく黄を保持してから後半でゆっくり消える（EaseInで余韻）。
    lockOnFlash_->PlayAlphaAnimation(1.0f, 0.0f, 1.0f, Easing::Function::EaseInQuad, false);
    // ③ 大きく開いた照準が一気に締まって軽くオーバーシュート＝「カチッとロック」
    lockOnFlash_->PlayScaleAnimation(Vector2{ 1.8f, 1.8f }, Vector2{ 1.0f, 1.0f },
        0.30f, Easing::Function::EaseOutBack, false);
}
