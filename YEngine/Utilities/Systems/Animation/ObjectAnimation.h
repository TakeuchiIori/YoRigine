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

    Vector3 startScale;
    Vector3 targetScale;

    Vector4 startColor;
    Vector4 targetColor;

    Vector3 startRotation;
    Vector3 targetRotation;

    Vector3 startPosition;
    Vector3 targetPosition;

    bool isActive = false;
    std::function<void()> onComplete = nullptr;
};

class ObjectAnimation {
public:
    ObjectAnimation() = default;
    ~ObjectAnimation() = default;

    void StartScaleAnimation(
        Vector3 fromScale, Vector3 toScale, float duration,
        Easing::Function easeFunc = Easing::Function::EaseOutQuad,
        std::function<void()> onComplete = nullptr
    );

    // baseScale_ を基準にした相対倍率でスケールアニメーション。
    // 例: {1.1, 0.9, 0.7} は baseScale_ を各軸に対して 1.1/0.9/0.7 倍した値。
    // フィールドから引き継がれたスケールを保持したまま変形させたいときに使う。
    void StartRelativeScaleAnimation(
        Vector3 fromRelative, Vector3 toRelative, float duration,
        Easing::Function easeFunc = Easing::Function::EaseOutQuad,
        std::function<void()> onComplete = nullptr
    );

    void StartColorAnimation(
        Vector4 fromColor, Vector4 toColor, float duration,
        Easing::Function easeFunc = Easing::Function::Linear,
        std::function<void()> onComplete = nullptr
    );

    // 新たに追加：回転アニメーション
    void StartRotationAnimation(
        Vector3 fromRotation, Vector3 toRotation, float duration,
        Easing::Function easeFunc = Easing::Function::EaseOutQuad,
        std::function<void()> onComplete = nullptr
    );

    // 新たに追加：位置アニメーション
    void StartPositionAnimation(
        Vector3 fromPosition, Vector3 toPosition, float duration,
        Easing::Function easeFunc = Easing::Function::EaseOutQuad,
        std::function<void()> onComplete = nullptr
    );

    void PlayPunchAnimation(float strength = 0.2f, float duration = 0.3f);
    void PlayShakeAnimation(float intensity = 0.1f, float duration = 0.2f);
    void PlayBounceScaleAnimation(float targetScale = 1.2f, float duration = 0.5f);

    void Update(float dt);
    void StopAll();

    Vector3 GetCurrentScale() const { return currentScale_; }
    Vector4 GetCurrentColor() const { return currentColor_; }
    Vector3 GetCurrentRotation() const { return currentRotation_; }
    Vector3 GetCurrentPosition() const { return currentPosition_; }

    Vector3 GetBaseScale() const { return baseScale_; }

    void SetBaseScale(const Vector3& scale) { baseScale_ = scale; currentScale_ = scale; }
    void SetBaseColor(const Vector4& color) { baseColor_ = color; currentColor_ = color; }
    void SetBaseRotation(const Vector3& rotation) { baseRotation_ = rotation; currentRotation_ = rotation; }
    void SetBasePosition(const Vector3& position) { basePosition_ = position; currentPosition_ = position; }

    bool IsAnimating() const;
    bool IsScaleAnimating() const { return scaleAnim_.isActive; }
    bool IsColorAnimating() const { return colorAnim_.isActive; }
    bool IsRotationAnimating() const { return rotationAnim_.isActive; }
    bool IsPositionAnimating() const { return positionAnim_.isActive; }

private:
    AnimationData scaleAnim_;
    AnimationData colorAnim_;
    AnimationData rotationAnim_;
    AnimationData positionAnim_;

    Vector3 currentScale_ = { 1.0f, 1.0f, 1.0f };
    Vector4 currentColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 currentRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 currentPosition_ = { 0.0f, 0.0f, 0.0f };

    Vector3 baseScale_ = { 1.0f, 1.0f, 1.0f };
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 baseRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };

    float shakeTimer_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeIntensity_ = 0.0f;
    bool isShaking_ = false;

    void UpdateAnimation(AnimationData& anim, float dt);
};