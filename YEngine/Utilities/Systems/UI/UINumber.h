#pragma once
#include "UIBase.h"
#include "Sprite/Sprite.h"
#include <vector>
#include <memory>
#include <string>

/// <summary>
/// 連番数字スプライトシートから数値を表示する UI クラス
///
/// UIBase を継承しているため、以下がそのまま使える:
///   - PlayFadeIn / PlayFadeOut / PlaySlideIn 等の全アニメーション
///   - UIManager::AddUI() で管理可能（UIBase* として扱える）
///   - SetColor / SetAlpha / SetPosition 等の共通 API
///
/// 前提: 0〜9 が横1列に並んだスプライトシートを使用
///   例) 1280 x 128 のテクスチャ → 1桁 = 128 x 128
///
/// 基本的な使い方:
///   auto num = std::make_unique<UINumber>("Score");
///   num->Initialize("resources/numbers.png");
///   num->SetNumber(42);
///   num->PlayFadeIn(0.5f);   // ← UIBase のアニメがそのまま動く
///   UIManager::GetInstance()->AddUI("score", std::move(num));
/// </summary>
class UINumber : public UIBase {
public:
    /// <summary>桁の水平アライメント</summary>
    enum class Alignment { Left, Center, Right };

public:
    explicit UINumber(const std::string& name = "UINumber");
    ~UINumber() = default;

    // -------------------------------------------------------------------------
    // 初期化 / 更新 / 描画  （UIBase をオーバーライド）
    // -------------------------------------------------------------------------

    /// <summary>
    /// 初期化（UIBase::Initialize の代わりにこちらを呼ぶ）
    /// </summary>
    /// <param name="textureFilePath">0〜9 が横並びのスプライトシートパス</param>
    /// <param name="maxDigits">最大表示桁数（デフォルト 8）</param>
    void Initialize(const std::string& textureFilePath, int maxDigits = 8);

    /// <summary>
    /// 毎フレーム更新
    /// UIBase::Update() でアニメーション・Transform を処理し、
    /// 結果を各桁スプライトに伝播する
    /// </summary>
    void Update() override;

    /// <summary>
    /// 描画（桁スプライトのみ描画。UIBase の sprite_ は使わない）
    /// </summary>
    void Draw() override;

    // -------------------------------------------------------------------------
    // 数値制御
    // -------------------------------------------------------------------------

    /// <summary>表示する整数値をセット（負数は 0 として扱う）</summary>
    void SetNumber(int value);

    /// <summary>現在の数値を取得</summary>
    int GetNumber() const { return currentValue_; }

    /// <summary>
    /// 桁数を固定（ゼロ埋め）
    ///   0 = 自動（"42"）  3 = 固定（"042"）
    /// </summary>
    void SetFixedDigits(int digits);
    int GetFixedDigits() const { return fixedDigits_; }

    // -------------------------------------------------------------------------
    // レイアウト制御
    // -------------------------------------------------------------------------

    /// 1桁の表示サイズ（ピクセル）
    void SetDigitSize(const Vector2& size);
    Vector2 GetDigitSize() const { return digitSize_; }

    // 桁間の追加スペース（ピクセル
    void SetSpacing(float spacing);
    float GetSpacing() const { return spacing_; }

    /// アライメント（SetPosition の基準点に対する文字揃え）
    ///   Left   → 左端が基準（デフォルト）
    ///   Center → 中央が基準
    ///   Right  → 右端が基準（スコア表示などに便利）
    void SetAlignment(Alignment align);
    Alignment GetAlignment() const { return alignment_; }

private:
    // -------------------------------------------------------------------------
    // 内部処理
    // -------------------------------------------------------------------------

    /// 現在の数値に合わせて桁スプライトを生成/更新する
    /// SetNumber や SetFixedDigits 変更時に呼ぶ
    void RebuildDigits();

    /// <summary>
    /// UIBase の現在 Transform（アニメーション適用済み）を基準に
    /// 各桁スプライトの位置・色・アルファを同期する
    /// </summary>
    void SyncDigitSprites();

    /// 指定スプライトに digit（0〜9）の UV を設定する
    ///   TextureLeftTop.x = digit * digitTexWidth_
    ///   TextureSize      = { digitTexWidth_, digitTexHeight_ }
    void ApplyDigitUV(Sprite* sprite, int digit) const;

    // -------------------------------------------------------------------------
    // メンバ変数
    // -------------------------------------------------------------------------

    /// 桁ごとのスプライト（index 0 = 最上位桁）
    std::vector<std::unique_ptr<Sprite>> digitSprites_;

    std::string numberTexturePath_;

    int currentValue_ = 0;
    int fixedDigits_ = 0;   // 0 = 自動
    int maxDigits_ = 8;

    Vector2   digitSize_ = { 64.0f, 64.0f };
    float     spacing_ = 0.0f;
    Alignment alignment_ = Alignment::Left;

    // テクスチャの1桁サイズ（ピクセル）
    float digitTexWidth_ = 0.0f;
    float digitTexHeight_ = 0.0f;
};