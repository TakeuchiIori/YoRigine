#include "UINumber.h"
#include "Loaders/Texture/TextureManager.h"

//=============================================================================
// コンストラクタ
//=============================================================================
UINumber::UINumber(const std::string& name)
    : UIBase(name)   // 親クラスのコンストラクタで name_ を設定
{
}

//=============================================================================
// 初期化
//=============================================================================
void UINumber::Initialize(const std::string& textureFilePath, int maxDigits)
{
    numberTexturePath_ = textureFilePath;
    maxDigits_ = maxDigits;

    // ─── テクスチャを読み込み、1桁サイズを算出 ───────────────────
    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    const DirectX::TexMetadata& meta =
        TextureManager::GetInstance()->GetMetaData(textureFilePath);

    digitTexWidth_ = static_cast<float>(meta.width) / 10.0f;
    digitTexHeight_ = static_cast<float>(meta.height);
    digitSize_ = { digitTexWidth_, digitTexHeight_ };

    // ─── UIBase の sprite_ を初期化 ──────────────────────────────
    // アニメーション系は sprite_ の Transform / Color を読み書きするため、
    // sprite_ を有効な状態にしておく必要がある。
    // UINumber では sprite_ を直接描画しない（Draw() をオーバーライド済み）
    sprite_ = std::make_unique<YoRigine::Sprite>();
    sprite_->Initialize(textureFilePath);
    texturePath_ = textureFilePath;

    // ─── 初期桁スプライトを生成 ──────────────────────────────────
    RebuildDigits();
}

//=============================================================================
// 毎フレーム更新
//=============================================================================
void UINumber::Update()
{
    // UIBase::Update() でアニメーション・Transform・UV 等を処理
    // → sprite_->translate_ / color / alpha がアニメーション値に更新される
    UIBase::Update();

    // UIBase が計算した最新 Transform・Color を桁スプライトへ伝播
    SyncDigitSprites();
}

//=============================================================================
// 描画
//=============================================================================
void UINumber::Draw()
{
    if (!IsVisible()) return;

    // UIBase::Draw() は呼ばない（sprite_ は描画コンテナとしてのみ使用）
    for (auto& s : digitSprites_) {
        s->Draw();
    }
}

//=============================================================================
// 数値をセット
//=============================================================================
void UINumber::SetNumber(int value)
{
    currentValue_ = (value < 0) ? 0 : value;
    RebuildDigits();
}

//=============================================================================
// 固定桁数をセット
//=============================================================================
void UINumber::SetFixedDigits(int digits)
{
    fixedDigits_ = digits;
    RebuildDigits();
}

//=============================================================================
// 1桁の表示サイズをセット
//=============================================================================
void UINumber::SetDigitSize(const Vector2& size)
{
    digitSize_ = size;
    // 全桁に反映（SyncDigitSprites で位置も再計算される）
    for (auto& s : digitSprites_) {
        s->SetSize(size);
    }
}

//=============================================================================
// 桁間スペースをセット
//=============================================================================
void UINumber::SetSpacing(float spacing)
{
    spacing_ = spacing;
}

//=============================================================================
// アライメントをセット
//=============================================================================
void UINumber::SetAlignment(Alignment align)
{
    alignment_ = align;
}

//=============================================================================
// private: 桁スプライトを生成・再構成
//=============================================================================
void UINumber::RebuildDigits()
{
    // ─── 表示する各桁の数字を配列に変換 ──────────────────────────
    // 例: value=42, fixedDigits=0  → [4, 2]
    //     value=7,  fixedDigits=3  → [0, 0, 7]
    std::vector<int> digitValues;

    if (currentValue_ == 0) {
        digitValues.push_back(0);
    }
    else {
        int v = currentValue_;
        while (v > 0) {
            digitValues.insert(digitValues.begin(), v % 10);
            v /= 10;
        }
    }

    // ゼロ埋め（固定桁数）
    if (fixedDigits_ > 0) {
        while (static_cast<int>(digitValues.size()) < fixedDigits_) {
            digitValues.insert(digitValues.begin(), 0);
        }
    }

    // 最大桁数でクリップ
    if (static_cast<int>(digitValues.size()) > maxDigits_) {
        digitValues.erase(
            digitValues.begin(),
            digitValues.begin() + (static_cast<int>(digitValues.size()) - maxDigits_)
        );
    }

    const int needed = static_cast<int>(digitValues.size());

    // ─── スプライトを過不足なく確保 ────────────────────────────────
    // 足りない分を生成
    while (static_cast<int>(digitSprites_.size()) < needed) {
        auto sp = std::make_unique<YoRigine::Sprite>();
        sp->Initialize(numberTexturePath_);
        sp->SetSize(digitSize_);
        if (camera_) sp->SetCamera(camera_);
        digitSprites_.push_back(std::move(sp));
    }

    // 多すぎる分を削除
    while (static_cast<int>(digitSprites_.size()) > needed) {
        digitSprites_.pop_back();
    }

    // ─── 各桁に UV をセット ─────────────────────────────────────
    for (int i = 0; i < needed; ++i) {
        ApplyDigitUV(digitSprites_[i].get(), digitValues[i]);
    }
}

//=============================================================================
// UIBase の Transform・Color を桁スプライトへ同期
//=============================================================================
void UINumber::SyncDigitSprites()
{
    if (digitSprites_.empty()) return;

    // UIBase（アニメーション適用済み）の現在値を取得
    const Vector3 basePos = GetPosition();    // sprite_->GetTranslate()
    const Vector4 baseColor = GetColor();       // sprite_->GetColor()

    const int   count = static_cast<int>(digitSprites_.size());
    const float stepX = digitSize_.x + spacing_;
    const float totalWidth = stepX * count - spacing_;

    // アライメントによる先頭 X オフセット
    float startX = basePos.x;
    switch (alignment_) {
    case Alignment::Center: startX -= totalWidth * 0.5f; break;
    case Alignment::Right:  startX -= totalWidth;        break;
    default: /* Left */                                  break;
    }

    for (int i = 0; i < count; ++i) {
        YoRigine::Sprite* s = digitSprites_[i].get();

        // 位置（アニメーション後の基準位置 + 桁オフセット）
        s->SetTranslate({
            startX + stepX * static_cast<float>(i),
            basePos.y,
            basePos.z
            });

        // 色・アルファ（UIBase のアニメーションが FadeIn/Color 系を制御）
        s->SetColor(baseColor);

        s->Update();
    }
}

//=============================================================================
// private: 指定スプライトに digit（0〜9）の UV をセット
//=============================================================================
void UINumber::ApplyDigitUV(YoRigine::Sprite* sprite, int digit) const
{
    // テクスチャ上の切り出し左上座標（ピクセル）
    sprite->SetTextureLeftTop({
        static_cast<float>(digit) * digitTexWidth_,
        0.0f
        });

    // 切り出しサイズ（ピクセル）
    sprite->SetTextureSize({ digitTexWidth_, digitTexHeight_ });
}