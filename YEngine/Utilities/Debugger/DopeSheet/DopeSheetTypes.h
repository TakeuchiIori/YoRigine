#pragma once

#include <algorithm>
#include <cstdint>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace DopeSheet
{

//=============================================================================
// Color
// DopeSheet 専用の RGBA カラー型
//=============================================================================
struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {
    }

    // プリセット
    static constexpr Color White()  { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
    static constexpr Color Red()    { return { 1.0f, 0.3f, 0.3f, 1.0f }; }
    static constexpr Color Green()  { return { 0.3f, 1.0f, 0.4f, 1.0f }; }
    static constexpr Color Blue()   { return { 0.3f, 0.6f, 1.0f, 1.0f }; }
    static constexpr Color Yellow() { return { 1.0f, 0.9f, 0.2f, 1.0f }; }
    static constexpr Color Orange() { return { 1.0f, 0.55f,0.1f, 1.0f }; }
    static constexpr Color Purple() { return { 0.7f, 0.3f, 1.0f, 1.0f }; }
    static constexpr Color Cyan()   { return { 0.2f, 0.9f, 0.9f, 1.0f }; }
    static constexpr Color Gray()   { return { 0.6f, 0.6f, 0.6f, 1.0f }; }

    Color WithAlpha(float alpha) const { return { r, g, b, alpha }; }

    Color Brightened(float amount = 0.3f) const
    {
        return {
            std::min(r + amount, 1.0f),
            std::min(g + amount, 1.0f),
            std::min(b + amount, 1.0f),
            a
        };
    }

#ifdef USE_IMGUI
    ImVec4 ToImVec4() const { return { r, g, b, a }; }
    ImU32  ToImU32()  const { return ImGui::ColorConvertFloat4ToU32(ToImVec4()); }
#endif
};

//=============================================================================
// KeyShape
// キーフレームの描画形状
//=============================================================================
enum class KeyShape : uint8_t
{
    Diamond,  // ◆ ひし形（デフォルト）
    Bar,      // ██ 区間バー
    Circle,   // ● 丸
    Triangle, // ▲ 三角
};

//=============================================================================
// TrackType
// トラックの意味づけ
//=============================================================================
enum class TrackType : uint8_t
{
    Generic = 0,

    // モーション系
    MotionTranslate,
    MotionRotate,
    MotionScale,

    // コンボ・アクション系
    AttackHitbox,
    InvincibleFrame,
    ArmorFrame,
    ComboWindow,
    CancelWindow,
    CounterWindow,

    // エフェクト・SE 系
    Effect,
    Sound,
    CameraShake,

    // その他
    Event,
    Custom,
};

// TrackType ごとのデフォルト色
inline Color GetDefaultTrackColor(TrackType type)
{
    switch (type)
    {
    case TrackType::MotionTranslate:  return Color::Blue();
    case TrackType::MotionRotate:     return Color::Green();
    case TrackType::MotionScale:      return Color::Purple();
    case TrackType::AttackHitbox:     return Color::Red();
    case TrackType::InvincibleFrame:  return Color::Cyan();
    case TrackType::ArmorFrame:       return Color::Yellow();
    case TrackType::ComboWindow:      return Color::Orange();
    case TrackType::CancelWindow:     return { 1.0f, 0.75f, 0.4f };
    case TrackType::CounterWindow:    return { 0.5f, 1.0f,  0.6f };
    case TrackType::Effect:           return { 0.9f, 0.5f,  1.0f };
    case TrackType::Sound:            return { 0.4f, 0.85f, 1.0f };
    case TrackType::CameraShake:      return { 1.0f, 0.8f,  0.3f };
    case TrackType::Event:            return Color::Gray();
    default:                          return Color::White();
    }
}

// TrackType ごとのデフォルトラベル
inline const char* GetDefaultTrackLabel(TrackType type)
{
    switch (type)
    {
    case TrackType::MotionTranslate:  return "Translate";
    case TrackType::MotionRotate:     return "Rotate";
    case TrackType::MotionScale:      return "Scale";
    case TrackType::AttackHitbox:     return "HitBox";
    case TrackType::InvincibleFrame:  return "Invincible";
    case TrackType::ArmorFrame:       return "Armor";
    case TrackType::ComboWindow:      return "Combo Window";
    case TrackType::CancelWindow:     return "Cancel Window";
    case TrackType::CounterWindow:    return "Counter Window";
    case TrackType::Effect:           return "Effect";
    case TrackType::Sound:            return "Sound";
    case TrackType::CameraShake:      return "CameraShake";
    case TrackType::Event:            return "Event";
    default:                          return "Track";
    }
}

// TrackType ごとのアイコン文字
inline const char* GetTrackIcon(TrackType type)
{
    switch (type)
    {
    case TrackType::MotionTranslate:  return "[T]";
    case TrackType::MotionRotate:     return "[R]";
    case TrackType::MotionScale:      return "[Z]";
    case TrackType::AttackHitbox:     return "[H]";
    case TrackType::InvincibleFrame:  return "[I]";
    case TrackType::ArmorFrame:       return "[A]";
    case TrackType::ComboWindow:      return "[C]";
    case TrackType::CancelWindow:     return "[X]";
    case TrackType::CounterWindow:    return "[K]";
    case TrackType::Effect:           return "[E]";
    case TrackType::Sound:            return "[S]";
    case TrackType::CameraShake:      return "[Q]";
    case TrackType::Event:            return "[V]";
    default:                          return "[ ]";
    }
}

} // namespace DopeSheet
