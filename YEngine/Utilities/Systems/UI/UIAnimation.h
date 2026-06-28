#pragma once

// Math
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Easing.h"

// C++
#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <json.hpp>

// ============================================================
// UIAnimTrackType
// 1トラック = 1プロパティのfloat1次元
// ============================================================
enum class UIAnimTrackType : uint8_t {
	Alpha,
	PosX,PosY,
	ScaleX,ScaleY,
	RotationZ,
	R,G,B
};

// トラックタイプのラベル（エディタで表示用）
inline const char* UIAnimTrackTypeLabel(UIAnimTrackType t) {
	switch (t) {
	case UIAnimTrackType::Alpha:        return "Alpha";
	case UIAnimTrackType::PosX:         return "PosX";
    case UIAnimTrackType::PosY:         return "PosY";
    case UIAnimTrackType::ScaleX:       return "Scale X";
    case UIAnimTrackType::ScaleY:       return "Scale Y";
    case UIAnimTrackType::RotationZ:    return "Rotation Z";
    case UIAnimTrackType::R:            return "Color R";
    case UIAnimTrackType::G:            return "Color G";
    case UIAnimTrackType::B:            return "Color B";
    default:                            return "Unknown";
    }
}

// ============================================================
// UIAnimKeyframe
// 正規化時間 [0,1] 上の1点。
// ============================================================
struct UIAnimKeyframe {
    float time = 0.0f;
    float val = 0.0f;
    Easing::Function easing = Easing::Function::Linear;
};

// ============================================================
// UIAnimTrack
// 1プロパティのキーフレーム列。Evaluate(t) で現在値を返す。
// ============================================================
struct UIAnimTrack {
    UIAnimTrackType type = UIAnimTrackType::Alpha;
    std::vector<UIAnimKeyframe> keyframes;

    // キーフレームの追加
    void AddKey(float time, float val, Easing::Function easing = Easing::Function::Linear) {
        keyframes.push_back({ time,val,easing });
        Sortkeyframes();
    }
    // キーフレームの削除
    void RemoveKey(int index) {
        // 範囲外を参照しないように
        if (index >= 0 && index < static_cast<int>(keyframes.size()))
            keyframes.erase(keyframes.begin() + index);
    }
    // キーフレームのソート
    void Sortkeyframes() {
        // 時間を基準にソート
        std::sort(keyframes.begin(), keyframes.end(),
            [](const UIAnimKeyframe& a, const UIAnimKeyframe& b) {return a.time < b.time; });
    }

    // 指定された時間に対応するアニメーション値を保管して返す
    float Evaluate(float t) const {
        // 例外処理:境界のチェック
        if (keyframes.empty()) return 0.0f;
        if (keyframes.size() == 1 || t <= keyframes.front().time) return keyframes.front().val;
        if (t >= keyframes.back().time) return keyframes.back().val;

        // 現在時刻tが含まれるキーフレーム区間を検索
        for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
            const auto& a = keyframes[i];
            const auto& b = keyframes[i + 1];

            if (a.time <= t && t <= b.time) {
                float range = b.time - a.time;
                // 同じ時刻のものなら現在の値を返す
                if (range < 1e-6f) return a.val;

				// 補間計算
                float localT = (t - a.time) / range;
                float easeT = Easing::Ease(a.easing, localT);

                return a.val + (b.val - a.val) * easeT;
            }
        }
        return keyframes.back().val;
    }
};

// ============================================================
// UIAnimTrigger
// クリップの自動再生タイミング。
// ============================================================
enum class UIAnimTrigger : uint8_t {
	Manual,   // コードから明示的に PlayAnimation("name") で再生
	OnInit,   // Initialize 完了時に自動再生
	OnShow,   // 非表示 → 表示に切り替わった時に自動再生
};

// ============================================================
// UIAnimationClip
// 複数トラックをまとめた「演出の単位」
// ============================================================
struct UIAnimationClip {
    std::string name = "Clip";
    float duration = 0.5f;
    bool loop = false;
    UIAnimTrigger trigger = UIAnimTrigger::Manual;
    std::vector<UIAnimTrack> tracks;

    // トラックの検索（無い == nullptr）
    UIAnimTrack* FindTrack(UIAnimTrackType type) {
        for (auto& t : tracks)
            if (t.type == type) return &t;
            return nullptr;
    }
    // タイプで検索
    const UIAnimTrack* FindTrack(UIAnimTrackType type) const {
        for (const auto& t : tracks)
            if (t.type == type) return &t;
        return nullptr;
    }

    // トラックの追加（同タイプがあるなら追加しない）
    UIAnimTrack& AddTrack(UIAnimTrackType type) {
        auto* existing = FindTrack(type);
        if (existing) return *existing;
        tracks.push_back({ type,{} });
        return tracks.back();
    }
};

// ============================================================
// UIAnimationClip <-> JSON  (nlohmann/json)
// ============================================================
inline void to_json(nlohmann::json& j, const UIAnimKeyframe& k) {
    j = nlohmann::json{
        { "time",   k.time   },
        { "val",  k.val  },
        { "easing", static_cast<int>(k.easing) },
    };
}
inline void from_json(const nlohmann::json& j, UIAnimKeyframe& k) {
    if (j.contains("time"))   j.at("time").get_to(k.time);
    if (j.contains("val"))  j.at("val").get_to(k.val);
    if (j.contains("easing")) k.easing = static_cast<Easing::Function>(j.at("easing").get<int>());
}

inline void to_json(nlohmann::json& j, const UIAnimTrack& t) {
    j = nlohmann::json{
        { "type", static_cast<int>(t.type) },
        { "keyframes", t.keyframes                   },
    };
}
inline void from_json(const nlohmann::json& j, UIAnimTrack& t) {
    if (j.contains("type")) t.type = static_cast<UIAnimTrackType>(j.at("type").get<int>());
    if (j.contains("keyframes")) j.at("keyframes").get_to(t.keyframes);
}

inline void to_json(nlohmann::json& j, const UIAnimationClip& c) {
    j = nlohmann::json{
        { "name",     c.name                       },
        { "duration", c.duration                   },
        { "loop",     c.loop                       },
        { "trigger",  static_cast<int>(c.trigger)  },
        { "tracks",   c.tracks                     },
    };
}
inline void from_json(const nlohmann::json& j, UIAnimationClip& c) {
    if (j.contains("name"))     j.at("name").get_to(c.name);
    if (j.contains("duration")) j.at("duration").get_to(c.duration);
    if (j.contains("loop"))     j.at("loop").get_to(c.loop);
    if (j.contains("trigger"))  c.trigger = static_cast<UIAnimTrigger>(j.at("trigger").get<int>());
    if (j.contains("tracks"))   j.at("tracks").get_to(c.tracks);
}

// ============================================================
// 後方互換：旧 UIAnimation / UIAnimationType は UIAnimator の
// プリセット系 PlayFadeIn() 等がまだ使うため残しておく。
// UIAnimationClip の新設計とは分離されている。
// ============================================================
enum class UIAnimationType {
    None,
    Position, Scale, Rotation, Color, Alpha,
    FadeIn, FadeOut, SlideIn, SlideOut, ZoomIn, ZoomOut,
    Shake, Pulse, Bounce, Swing, Flash, Blink, Wobble, Flip, RotateIn, RotateOut
};

enum class SlideDirection { Left, Right, Up, Down };

struct UIAnimation {
    UIAnimationType  type = UIAnimationType::None;
    Easing::Function easingType = Easing::Function::Linear;
    float duration = 1.0f;
    float elapsed = 0.0f;
    float delay = 0.0f;
    bool  loop = false;
    bool  pingpong = false;
    bool  isReversing = false;

    Vector3 startPos, endPos;
    Vector2 startScale, endScale;
    Vector3 startRotation, endRotation;
    Vector4 startColor, endColor;
    float   startAlpha = 0.0f, endAlpha = 1.0f;

    float intensity = 1.0f;
    float frequency = 1.0f;
    Vector3 originalPos;
    Vector2 originalScale;

    std::function<void()> onComplete;
    std::function<void()> onUpdate;
};

// ============================================================
// UIAnimationPresets（プリセット系 Play〜 が内部で使う、変更なし）
// ============================================================
namespace UIAnimationPresets {
    inline UIAnimation FadeIn(float duration = 1.0f, Easing::Function easing = Easing::Function::EaseOutQuad) {
        UIAnimation a; a.type = UIAnimationType::FadeIn; a.duration = duration; a.easingType = easing;
        a.startAlpha = 0.0f; a.endAlpha = 1.0f; return a;
    }
    inline UIAnimation FadeOut(float duration = 1.0f, Easing::Function easing = Easing::Function::EaseInQuad) {
        UIAnimation a; a.type = UIAnimationType::FadeOut; a.duration = duration; a.easingType = easing;
        a.startAlpha = 1.0f; a.endAlpha = 0.0f; return a;
    }
    inline UIAnimation SlideIn(SlideDirection dir, float distance, float duration = 0.5f) {
        UIAnimation a; a.type = UIAnimationType::SlideIn; a.duration = duration;
        a.easingType = Easing::Function::EaseOutCubic; a.intensity = distance;
        switch (dir) {
        case SlideDirection::Left:  a.startPos = { distance,  0, 0 }; break;
        case SlideDirection::Right: a.startPos = { -distance, 0, 0 }; break;
        case SlideDirection::Up:    a.startPos = { 0,  distance, 0 }; break;
        case SlideDirection::Down:  a.startPos = { 0, -distance, 0 }; break;
        }
        a.endPos = { 0, 0, 0 }; return a;
    }
    inline UIAnimation SlideOut(SlideDirection dir, float distance, float duration = 0.5f) {
        UIAnimation a; a.type = UIAnimationType::SlideOut; a.duration = duration;
        a.easingType = Easing::Function::EaseInCubic; a.intensity = distance;
        a.startPos = { 0, 0, 0 };
        switch (dir) {
        case SlideDirection::Left:  a.endPos = { -distance, 0, 0 }; break;
        case SlideDirection::Right: a.endPos = { distance, 0, 0 }; break;
        case SlideDirection::Up:    a.endPos = { 0, -distance, 0 }; break;
        case SlideDirection::Down:  a.endPos = { 0,  distance, 0 }; break;
        }
        return a;
    }
    inline UIAnimation ZoomIn(float duration = 0.5f) {
        UIAnimation a; a.type = UIAnimationType::ZoomIn; a.duration = duration;
        a.easingType = Easing::Function::EaseOutBack;
        a.startScale = { 0, 0 }; a.endScale = { 1, 1 };
        a.startAlpha = 0.0f;    a.endAlpha = 1.0f; return a;
    }
    inline UIAnimation ZoomOut(float duration = 0.5f) {
        UIAnimation a; a.type = UIAnimationType::ZoomOut; a.duration = duration;
        a.easingType = Easing::Function::EaseInBack;
        a.startScale = { 1, 1 }; a.endScale = { 0, 0 };
        a.startAlpha = 1.0f;    a.endAlpha = 0.0f; return a;
    }
    inline UIAnimation Shake(float intensity = 10.0f, float duration = 0.5f) {
        UIAnimation a; a.type = UIAnimationType::Shake; a.duration = duration;
        a.intensity = intensity; a.frequency = 20.0f; return a;
    }
    inline UIAnimation Pulse(float scale = 1.2f, float duration = 0.5f, bool loop = true) {
        UIAnimation a; a.type = UIAnimationType::Pulse; a.duration = duration;
        a.easingType = Easing::Function::EaseInOutSine;
        a.startScale = { 1, 1 }; a.endScale = { scale, scale };
        a.loop = loop; a.pingpong = true; return a;
    }
    inline UIAnimation Bounce(float height = 50.0f, float duration = 1.0f) {
        UIAnimation a; a.type = UIAnimationType::Bounce; a.duration = duration;
        a.easingType = Easing::Function::EaseOutBounce;
        a.startPos = { 0, -height, 0 }; a.endPos = { 0, 0, 0 }; return a;
    }
    inline UIAnimation Swing(float angle = 15.0f, float duration = 0.5f, bool loop = true) {
        UIAnimation a; a.type = UIAnimationType::Swing; a.duration = duration;
        a.easingType = Easing::Function::EaseInOutSine;
        a.startRotation = { 0, 0, -angle }; a.endRotation = { 0, 0, angle };
        a.loop = loop; a.pingpong = true; return a;
    }
    inline UIAnimation Flash(float duration = 0.5f, int times = 3) {
        UIAnimation a; a.type = UIAnimationType::Flash; a.duration = duration;
        a.frequency = static_cast<float>(times); return a;
    }
    inline UIAnimation Blink(float duration = 2.0f, bool loop = true) {
        UIAnimation a; a.type = UIAnimationType::Blink; a.duration = duration;
        a.easingType = Easing::Function::EaseInOutQuad;
        a.startAlpha = 1.0f; a.endAlpha = 0.3f;
        a.loop = loop; a.pingpong = true; return a;
    }
    inline UIAnimation Wobble(float intensity = 20.0f, float duration = 1.0f) {
        UIAnimation a; a.type = UIAnimationType::Wobble; a.duration = duration;
        a.intensity = intensity; a.frequency = 4.0f; return a;
    }
    inline UIAnimation Flip(bool horizontal = true, float duration = 0.6f) {
        UIAnimation a; a.type = UIAnimationType::Flip; a.duration = duration;
        a.easingType = Easing::Function::EaseInOutCubic;
        if (horizontal) { a.startScale = { 1, 1 }; a.endScale = { -1,  1 }; }
        else { a.startScale = { 1, 1 }; a.endScale = { 1, -1 }; }
        return a;
    }
    inline UIAnimation RotateIn(float duration = 0.8f) {
        UIAnimation a; a.type = UIAnimationType::RotateIn; a.duration = duration;
        a.easingType = Easing::Function::EaseOutBack;
        a.startRotation = { 0, 0, -180 }; a.endRotation = { 0, 0, 0 };
        a.startScale = { 0, 0 }; a.endScale = { 1, 1 };
        a.startAlpha = 0.0f; a.endAlpha = 1.0f; return a;
    }
    inline UIAnimation RotateOut(float duration = 0.8f) {
        UIAnimation a; a.type = UIAnimationType::RotateOut; a.duration = duration;
        a.easingType = Easing::Function::EaseInBack;
        a.startRotation = { 0, 0, 0 }; a.endRotation = { 0, 0, 180 };
        a.startScale = { 1, 1 }; a.endScale = { 0, 0 };
        a.startAlpha = 1.0f; a.endAlpha = 0.0f; return a;
    }
}
