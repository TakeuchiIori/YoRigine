#pragma once
#include <cstdint>

/// <summary>
/// カーブの補間方法
/// ※ ImGui 非依存 — リリースビルドでも使用可
/// </summary>
enum class InterpolationMode : uint8_t {
    Step,        // 前の値を維持（階段状）
    Linear,      // 線形補間
    CatmullRom,  // Catmull-Rom スプライン（タンジェント自動計算・滑らか）
    Bezier       // 三次ベジェ（inTangent / outTangent を使用）
};

/// <summary>
/// カーブの 1 キーフレーム。
/// time は任意範囲だが、パーティクルモジュールでは 0.0〜1.0 の正規化寿命を想定。
/// </summary>
struct CurveKey {
    float             time        = 0.0f;
    float             value       = 0.0f;
    float             inTangent   = 0.0f;   // Bezier 入力タンジェント（傾き）
    float             outTangent  = 0.0f;   // Bezier 出力タンジェント
    InterpolationMode interpMode  = InterpolationMode::Linear;

    CurveKey() = default;

    CurveKey(float t, float v,
             InterpolationMode mode = InterpolationMode::Linear,
             float inT  = 0.0f,
             float outT = 0.0f)
        : time(t), value(v)
        , inTangent(inT), outTangent(outT)
        , interpMode(mode)
    {}

    bool operator<(const CurveKey& rhs) const noexcept { return time < rhs.time; }
};
