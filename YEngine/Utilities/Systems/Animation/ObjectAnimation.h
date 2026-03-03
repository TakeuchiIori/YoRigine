#pragma once
#include <functional>
#include <memory>
#include <vector>

// Math
#include "Vector3.h"
#include "Vector4.h"
#include "Easing.h"



// アニメーションの種類
enum class AnimationType {
    Scale,
    Color,
    Rotation,
    Position
};

// アニメーションデータ構造
struct AnimationData {
    AnimationType type;
    float timer = 0.0f;
    float duration = 0.0f;
    Easing::Function easeFunc = Easing::Function::Linear;

    // スケール用
    Vector3 startScale;
    Vector3 targetScale;

    // カラー用
    Vector4 startColor;
    Vector4 targetColor;

    // 回転用
    Vector3 startRotation;
    Vector3 targetRotation;

    // 位置用
    Vector3 startPosition;
    Vector3 targetPosition;

    bool isActive = false;
    std::function<void()> onComplete = nullptr;
};

/// <summary>
/// 汎用3Dオブジェクトアニメーター
/// スケール、カラー、回転、位置のアニメーションを管理
/// </summary>
class ObjectAnimation {
public:
    ObjectAnimation() = default;
    ~ObjectAnimation() = default;

    /// <summary>
    /// スケールアニメーションを開始
    /// </summary>
    void StartScaleAnimation(
        Vector3 fromScale,
        Vector3 toScale,
        float duration,
        Easing::Function easeFunc = Easing::Function::EaseOutQuad,
        std::function<void()> onComplete = nullptr
    );

    /// <summary>
    /// カラーアニメーションを開始
    /// </summary>
    void StartColorAnimation(
        Vector4 fromColor,
        Vector4 toColor,
        float duration,
        Easing::Function easeFunc = Easing::Function::Linear,
        std::function<void()> onComplete = nullptr
    );

    /// <summary>
    /// パンチアニメーション（ヒット時に一瞬大きくなる）
    /// </summary>
    void PlayPunchAnimation(float strength = 0.2f, float duration = 0.3f);

    /// <summary>
    /// シェイクアニメーション（揺れ）
    /// </summary>
    void PlayShakeAnimation(float intensity = 0.1f, float duration = 0.2f);

    /// <summary>
    /// バウンススケールアニメーション（弾むように拡大）
    /// </summary>
    void PlayBounceScaleAnimation(float targetScale = 1.2f, float duration = 0.5f);

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update(float dt);

    /// <summary>
    /// すべてのアニメーションを停止
    /// </summary>
    void StopAll();

    /// <summary>
    /// 現在のアニメーション値を取得
    /// </summary>
    Vector3 GetCurrentScale() const { return currentScale_; }
    Vector4 GetCurrentColor() const { return currentColor_; }
    Vector3 GetCurrentRotation() const { return currentRotation_; }
    Vector3 GetCurrentPosition() const { return currentPosition_; }

    /// <summary>
    /// ベース値を設定（アニメーションが適用されていない時の値）
    /// </summary>
    void SetBaseScale(const Vector3& scale) { baseScale_ = scale; }
    void SetBaseColor(const Vector4& color) { baseColor_ = color; }
    void SetBaseRotation(const Vector3& rotation) { baseRotation_ = rotation; }
    void SetBasePosition(const Vector3& position) { basePosition_ = position; }

    /// <summary>
    /// アニメーション中か判定
    /// </summary>
    bool IsAnimating() const;
    bool IsScaleAnimating() const { return scaleAnim_.isActive; }
    bool IsColorAnimating() const { return colorAnim_.isActive; }

private:
    // 各アニメーションデータ
    AnimationData scaleAnim_;
    AnimationData colorAnim_;
    AnimationData rotationAnim_;
    AnimationData positionAnim_;

    // 現在の値
    Vector3 currentScale_ = { 1.0f, 1.0f, 1.0f };
    Vector4 currentColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 currentRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 currentPosition_ = { 0.0f, 0.0f, 0.0f };

    // ベース値（アニメーションが適用されていない時の値）
    Vector3 baseScale_ = { 1.0f, 1.0f, 1.0f };
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 baseRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };

    // シェイク用
    float shakeTimer_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeIntensity_ = 0.0f;
    bool isShaking_ = false;

    // 個別のアニメーションを更新
    void UpdateAnimation(AnimationData& anim, float dt);
};