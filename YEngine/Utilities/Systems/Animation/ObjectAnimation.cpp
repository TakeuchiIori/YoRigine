#include "ObjectAnimation.h"
#include <random>

void ObjectAnimation::StartScaleAnimation(
    Vector3 fromScale,
    Vector3 toScale,
    float duration,
    Easing::Function easeFunc,
    std::function<void()> onComplete
) {
    scaleAnim_.type = AnimationType::Scale;
    scaleAnim_.timer = 0.0f;
    scaleAnim_.duration = duration;
    scaleAnim_.easeFunc = easeFunc;
    scaleAnim_.startScale = fromScale;
    scaleAnim_.targetScale = toScale;
    scaleAnim_.isActive = true;
    scaleAnim_.onComplete = onComplete;

    currentScale_ = fromScale;
}

void ObjectAnimation::StartColorAnimation(
    Vector4 fromColor,
    Vector4 toColor,
    float duration,
    Easing::Function easeFunc,
    std::function<void()> onComplete
) {
    colorAnim_.type = AnimationType::Color;
    colorAnim_.timer = 0.0f;
    colorAnim_.duration = duration;
    colorAnim_.easeFunc = easeFunc;
    colorAnim_.startColor = fromColor;
    colorAnim_.targetColor = toColor;
    colorAnim_.isActive = true;
    colorAnim_.onComplete = onComplete;

    currentColor_ = fromColor;
}

void ObjectAnimation::PlayPunchAnimation(float strength, float duration) {
    Vector3 punchScale = baseScale_ * (1.0f + strength);
    StartScaleAnimation(
        punchScale,
        baseScale_,
        duration,
        Easing::Function::EaseOutQuad
    );
}

void ObjectAnimation::PlayShakeAnimation(float intensity, float duration) {
    isShaking_ = true;
    shakeTimer_ = 0.0f;
    shakeDuration_ = duration;
    shakeIntensity_ = intensity;
}

void ObjectAnimation::PlayBounceScaleAnimation(float targetScale, float duration) {
    Vector3 bounceScale = baseScale_ * targetScale;
    StartScaleAnimation(
        bounceScale,
        baseScale_,
        duration,
        Easing::Function::EaseOutGrowBounce // 既存のカスタムイージング使用
    );
}

void ObjectAnimation::Update(float dt) {
    if (scaleAnim_.isActive) UpdateAnimation(scaleAnim_, dt);
    if (colorAnim_.isActive) UpdateAnimation(colorAnim_, dt);
    if (rotationAnim_.isActive) UpdateAnimation(rotationAnim_, dt);
    if (positionAnim_.isActive) UpdateAnimation(positionAnim_, dt);

    // シェイク更新
    if (isShaking_) {
        shakeTimer_ += dt;

        if (shakeTimer_ < shakeDuration_) {
            // ランダムなオフセットを生成
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

            // 時間経過で減衰
            float decay = 1.0f - (shakeTimer_ / shakeDuration_);
            float currentIntensity = shakeIntensity_ * decay;

            Vector3 shakeOffset = {
                dis(gen) * currentIntensity,
                dis(gen) * currentIntensity,
                dis(gen) * currentIntensity
            };

            currentPosition_ = basePosition_ + shakeOffset;
        }
        else {
            isShaking_ = false;
            currentPosition_ = basePosition_;
        }
    }
}

void ObjectAnimation::UpdateAnimation(AnimationData& anim, float dt) {
    anim.timer += dt;
    float t = std::min(anim.timer / anim.duration, 1.0f);

    // Easingクラスを使用して補間値を計算
    float easedT = Easing::Ease(anim.easeFunc, t);

    // アニメーションタイプに応じて値を更新
    switch (anim.type) {
    case AnimationType::Scale:
        currentScale_ = Lerp(anim.startScale, anim.targetScale, easedT);
        break;

    case AnimationType::Color:
        currentColor_ = Lerp(anim.startColor, anim.targetColor, easedT);
        break;

    case AnimationType::Rotation:
        currentRotation_ = Lerp(anim.startRotation, anim.targetRotation, easedT);
        break;

    case AnimationType::Position:
        currentPosition_ = Lerp(anim.startPosition, anim.targetPosition, easedT);
        break;
    }

    // アニメーション終了処理
    if (t >= 1.0f) {
        anim.isActive = false;

        if (anim.onComplete) {
            anim.onComplete();
        }
    }
}

void ObjectAnimation::StopAll() {
    scaleAnim_.isActive = false;
    colorAnim_.isActive = false;
    rotationAnim_.isActive = false;
    positionAnim_.isActive = false;
    isShaking_ = false;
}

bool ObjectAnimation::IsAnimating() const {
    return scaleAnim_.isActive ||
        colorAnim_.isActive ||
        rotationAnim_.isActive ||
        positionAnim_.isActive ||
        isShaking_;
}

void ObjectAnimation::StartRotationAnimation(Vector3 fromRotation, Vector3 toRotation, float duration, Easing::Function easeFunc, std::function<void()> onComplete) {
    rotationAnim_.type = AnimationType::Rotation;
    rotationAnim_.timer = 0.0f;
    rotationAnim_.duration = duration;
    rotationAnim_.easeFunc = easeFunc;
    rotationAnim_.startRotation = fromRotation;
    rotationAnim_.targetRotation = toRotation;
    rotationAnim_.isActive = true;
    rotationAnim_.onComplete = onComplete;
    currentRotation_ = fromRotation;
}

void ObjectAnimation::StartPositionAnimation(Vector3 fromPosition, Vector3 toPosition, float duration, Easing::Function easeFunc, std::function<void()> onComplete) {
    positionAnim_.type = AnimationType::Position;
    positionAnim_.timer = 0.0f;
    positionAnim_.duration = duration;
    positionAnim_.easeFunc = easeFunc;
    positionAnim_.startPosition = fromPosition;
    positionAnim_.targetPosition = toPosition;
    positionAnim_.isActive = true;
    positionAnim_.onComplete = onComplete;
    currentPosition_ = fromPosition;
}