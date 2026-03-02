#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Easing.h"
#include <functional>
#include <vector>
#include <memory>

///************************* アニメーションタイプ *************************///
enum class UIAnimationType {
    None,
    Position,       // 位置移動
    Scale,          // スケール変更
    Rotation,       // 回転
    Color,          // 色変更
    Alpha,          // アルファ値変更

    // 複合アニメーション
    FadeIn,         // フェードイン
    FadeOut,        // フェードアウト
    SlideIn,        // スライドイン
    SlideOut,       // スライドアウト
    ZoomIn,         // ズームイン
    ZoomOut,        // ズームアウト

    // エフェクト系
    Shake,          // 振動
    Pulse,          // パルス(拡大縮小)
    Bounce,         // バウンス
    Swing,          // スイング
    Flash,          // 点滅
    Blink,          // 瞬き
    Wobble,         // ぐらぐら揺れる
    Flip,           // 反転
    RotateIn,       // 回転しながら登場
    RotateOut       // 回転しながら退場
};

///************************* スライド方向 *************************///
enum class SlideDirection {
    Left,
    Right,
    Up,
    Down
};

///************************* アニメーション構造体 *************************///
struct UIAnimation {
    UIAnimationType type = UIAnimationType::None;
    Easing::Function easingType = Easing::Function::Linear;

    float duration = 1.0f;      // アニメーション時間
    float elapsed = 0.0f;       // 経過時間
    float delay = 0.0f;         // 遅延時間
    bool loop = false;          // ループするか
    bool pingpong = false;      // 往復するか
    bool isReversing = false;   // 逆再生中か(pingpong用)

    // 開始・終了値
    Vector3 startPos, endPos;
    Vector2 startScale, endScale;
    Vector3 startRotation, endRotation;
    Vector4 startColor, endColor;
    float startAlpha, endAlpha;

    // エフェクト用パラメータ
    float intensity = 1.0f;     // 強度
    float frequency = 1.0f;     // 周波数
    Vector3 originalPos;        // 元の位置(Shake等で使用)
    Vector2 originalScale;      // 元のスケール

    // コールバック
    std::function<void()> onComplete;
    std::function<void()> onUpdate;
};

///************************* プリセットアニメーション設定 *************************///
namespace UIAnimationPresets {
    // フェードイン (0→1)
    inline UIAnimation FadeIn(float duration = 1.0f, Easing::Function easing = Easing::Function::EaseOutQuad) {
        UIAnimation anim;
        anim.type = UIAnimationType::FadeIn;
        anim.duration = duration;
        anim.easingType = easing;
        anim.startAlpha = 0.0f;
        anim.endAlpha = 1.0f;
        return anim;
    }

    // フェードアウト (1→0)
    inline UIAnimation FadeOut(float duration = 1.0f, Easing::Function easing = Easing::Function::EaseInQuad) {
        UIAnimation anim;
        anim.type = UIAnimationType::FadeOut;
        anim.duration = duration;
        anim.easingType = easing;
        anim.startAlpha = 1.0f;
        anim.endAlpha = 0.0f;
        return anim;
    }

    // スライドイン
    inline UIAnimation SlideIn(SlideDirection dir, float distance, float duration = 0.5f) {
        UIAnimation anim;
        anim.type = UIAnimationType::SlideIn;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseOutCubic;
        anim.intensity = distance;

        // 方向に応じてオフセットを設定
        switch (dir) {
        case SlideDirection::Left:  anim.startPos = Vector3{ distance, 0, 0 }; break;
        case SlideDirection::Right: anim.startPos = Vector3{ -distance, 0, 0 }; break;
        case SlideDirection::Up:    anim.startPos = Vector3{ 0, distance, 0 }; break;
        case SlideDirection::Down:  anim.startPos = Vector3{ 0, -distance, 0 }; break;
        }
        anim.endPos = Vector3{ 0, 0, 0 };
        return anim;
    }

    // スライドアウト
    inline UIAnimation SlideOut(SlideDirection dir, float distance, float duration = 0.5f) {
        UIAnimation anim;
        anim.type = UIAnimationType::SlideOut;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInCubic;
        anim.intensity = distance;

        anim.startPos = Vector3{ 0, 0, 0 };
        switch (dir) {
        case SlideDirection::Left:  anim.endPos = Vector3{ -distance, 0, 0 }; break;
        case SlideDirection::Right: anim.endPos = Vector3{ distance, 0, 0 }; break;
        case SlideDirection::Up:    anim.endPos = Vector3{ 0, -distance, 0 }; break;
        case SlideDirection::Down:  anim.endPos = Vector3{ 0, distance, 0 }; break;
        }
        return anim;
    }

    // ズームイン
    inline UIAnimation ZoomIn(float duration = 0.5f) {
        UIAnimation anim;
        anim.type = UIAnimationType::ZoomIn;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseOutBack;
        anim.startScale = Vector2{ 0.0f, 0.0f };
        anim.endScale = Vector2{ 1.0f, 1.0f };
        anim.startAlpha = 0.0f;
        anim.endAlpha = 1.0f;
        return anim;
    }

    // ズームアウト
    inline UIAnimation ZoomOut(float duration = 0.5f) {
        UIAnimation anim;
        anim.type = UIAnimationType::ZoomOut;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInBack;
        anim.startScale = Vector2{ 1.0f, 1.0f };
        anim.endScale = Vector2{ 0.0f, 0.0f };
        anim.startAlpha = 1.0f;
        anim.endAlpha = 0.0f;
        return anim;
    }

    // シェイク
    inline UIAnimation Shake(float intensity = 10.0f, float duration = 0.5f) {
        UIAnimation anim;
        anim.type = UIAnimationType::Shake;
        anim.duration = duration;
        anim.intensity = intensity;
        anim.frequency = 20.0f;
        return anim;
    }

    // パルス(鼓動)
    inline UIAnimation Pulse(float scale = 1.2f, float duration = 0.5f, bool loop = true) {
        UIAnimation anim;
        anim.type = UIAnimationType::Pulse;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInOutSine;
        anim.startScale = Vector2{ 1.0f, 1.0f };
        anim.endScale = Vector2{ scale, scale };
        anim.loop = loop;
        anim.pingpong = true;
        return anim;
    }

    // バウンス
    inline UIAnimation Bounce(float height = 50.0f, float duration = 1.0f) {
        UIAnimation anim;
        anim.type = UIAnimationType::Bounce;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseOutBounce;
        anim.startPos = Vector3{ 0, -height, 0 };
        anim.endPos = Vector3{ 0, 0, 0 };
        return anim;
    }

    // スイング
    inline UIAnimation Swing(float angle = 15.0f, float duration = 0.5f, bool loop = true) {
        UIAnimation anim;
        anim.type = UIAnimationType::Swing;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInOutSine;
        anim.startRotation = Vector3{ 0, 0, -angle };
        anim.endRotation = Vector3{ 0, 0, angle };
        anim.loop = loop;
        anim.pingpong = true;
        return anim;
    }

    // フラッシュ(点滅)
    inline UIAnimation Flash(float duration = 0.5f, int times = 3) {
        UIAnimation anim;
        anim.type = UIAnimationType::Flash;
        anim.duration = duration;
        anim.frequency = static_cast<float>(times);
        return anim;
    }

    // ブリンク(瞬き)
    inline UIAnimation Blink(float duration = 2.0f, bool loop = true) {
        UIAnimation anim;
        anim.type = UIAnimationType::Blink;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInOutQuad;
        anim.startAlpha = 1.0f;
        anim.endAlpha = 0.3f;
        anim.loop = loop;
        anim.pingpong = true;
        return anim;
    }

    // ウォブル(ぐらぐら)
    inline UIAnimation Wobble(float intensity = 20.0f, float duration = 1.0f) {
        UIAnimation anim;
        anim.type = UIAnimationType::Wobble;
        anim.duration = duration;
        anim.intensity = intensity;
        anim.frequency = 4.0f;
        return anim;
    }

    // フリップ
    inline UIAnimation Flip(bool horizontal = true, float duration = 0.6f) {
        UIAnimation anim;
        anim.type = UIAnimationType::Flip;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInOutCubic;

        if (horizontal) {
            anim.startScale = Vector2{ 1.0f, 1.0f };
            anim.endScale = Vector2{ -1.0f, 1.0f };
        }
        else {
            anim.startScale = Vector2{ 1.0f, 1.0f };
            anim.endScale = Vector2{ 1.0f, -1.0f };
        }
        return anim;
    }

    // 回転登場
    inline UIAnimation RotateIn(float duration = 0.8f) {
        UIAnimation anim;
        anim.type = UIAnimationType::RotateIn;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseOutBack;
        anim.startRotation = Vector3{ 0, 0, -180.0f };
        anim.endRotation = Vector3{ 0, 0, 0 };
        anim.startScale = Vector2{ 0.0f, 0.0f };
        anim.endScale = Vector2{ 1.0f, 1.0f };
        anim.startAlpha = 0.0f;
        anim.endAlpha = 1.0f;
        return anim;
    }

    // 回転退場
    inline UIAnimation RotateOut(float duration = 0.8f) {
        UIAnimation anim;
        anim.type = UIAnimationType::RotateOut;
        anim.duration = duration;
        anim.easingType = Easing::Function::EaseInBack;
        anim.startRotation = Vector3{ 0, 0, 0 };
        anim.endRotation = Vector3{ 0, 0, 180.0f };
        anim.startScale = Vector2{ 1.0f, 1.0f };
        anim.endScale = Vector2{ 0.0f, 0.0f };
        anim.startAlpha = 1.0f;
        anim.endAlpha = 0.0f;
        return anim;
    }
}