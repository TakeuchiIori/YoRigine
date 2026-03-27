#pragma once
#include "CurveChannel.h"
#include <json.hpp>
#include <string>

/// <summary>
/// 1 つのフロート値を扱うプロパティ。
/// Constant / RandomBetween / Curve の 3 モードを統一インターフェースで提供。
///
/// ■ 複数チャンネルが必要な場合（RGBA など）
///   CurveProperty を 4 つ組み合わせて使う（UpdateColor 参照）。
///
/// ■ DrawEditor
///   USE_IMGUI 定義時のみ有効。
///   モードセレクタ + 各モードの入力 UI + カーブエディタ（Curve モード）を表示。
/// </summary>
class CurveProperty {
public:
    enum class Mode : uint8_t {
        Constant,       // valMin を固定値として使用
        RandomBetween,  // [valMin, valMax] の範囲でランダム
        Curve           // channel のカーブに沿った値
    };

    //===== 構築 =====
    CurveProperty();
    explicit CurveProperty(float constValue,
                           const std::string& channelName = "value");

    //===== 評価 =====
    /// @param t    0.0〜1.0 の正規化時刻（Curve モードで使用）
    /// @param rand 0.0〜1.0 のランダム値（RandomBetween モードで使用）
    float Evaluate(float t, float rand = 0.5f) const;

    //===== モード / 値 =====
    void  SetMode(Mode m)              { mode_ = m; }
    Mode  GetMode()              const { return mode_; }

    void  SetConstant(float v)         { valMin_ = valMax_ = v; mode_ = Mode::Constant; }
    void  SetRandom(float mn, float mx){ valMin_ = mn; valMax_ = mx; mode_ = Mode::RandomBetween; }

    float GetValMin()            const { return valMin_; }
    float GetValMax()            const { return valMax_; }
    void  SetValMin(float v)           { valMin_ = v; }
    void  SetValMax(float v)           { valMax_ = v; }

    //===== チャンネルアクセス =====
    CurveChannel&       GetChannel()       { return channel_; }
    const CurveChannel& GetChannel() const { return channel_; }

    //===== JSON =====
    nlohmann::json SaveToJson()                        const;
    void           LoadFromJson(const nlohmann::json& j);

#ifdef USE_IMGUI
    /// <summary>
    /// モードセレクタ + 値入力 / カーブエディタ を表示。
    /// </summary>
    /// @param label       ラベル文字列（ImGui::PushID に使用）
    /// @param editorHeight Curve モード時のエディタ高さ（px）
    /// @param dragSpeed   Constant/Random 時の DragFloat スピード
    /// @param valueMin    表示上の最小値
    /// @param valueMax    表示上の最大値
    void DrawEditor(const char* label       = "##CurveProp",
                    float       editorHeight = 120.0f,
                    float       dragSpeed    = 0.01f,
                    float       valueMin     = -0.1f,
                    float       valueMax     =  1.1f);
#endif

private:
    Mode         mode_   = Mode::Constant;
    float        valMin_ = 1.0f;
    float        valMax_ = 1.0f;
    CurveChannel channel_;
};
