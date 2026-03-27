#pragma once
#include "../../IParticleModule.h"
#include "../../../YParticleModuleFactory.h"
#include "Debugger/CurveEditor/CurveProperty.h"

#ifdef USE_IMGUI
#include <memory>
#include "Debugger/CurveEditor/CurveDelegate.h"
#endif

/// <summary>
/// ライフタイムに応じて色を変化させるモジュール。
///
/// ■ モード
///   TwoColor      : 開始色→終了色を線形補間（旧来の動作と互換）
///   GradientCurve : R / G / B / A を個別の CurveProperty でカーブ制御
///
/// ■ GradientCurve モード
///   各チャンネルは独立して Constant / RandomBetween / Curve を選択できる。
///   DrawEditor では RGBA 4 本のカーブを CurveDelegate で同一ビューに表示。
/// </summary>
class UpdateColor : public IUpdateModule
{
public:
    enum class ColorMode : uint8_t {
        TwoColor,       // 2色の線形補間
        GradientCurve   // R/G/B/A 個別カーブ
    };

    UpdateColor();

    //===== IUpdateModule =====
    void        OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) override;
    void        DrawEditor()                                                  override;
    std::string GetName()     const override { return "色"; }
    std::string GetTypeName() const override { return "UpdateColor"; }

    //===== シリアライズ =====
    void SaveToJson(nlohmann::json& json)        const override;
    void LoadFromJson(const nlohmann::json& json)      override;

    //===== アクセス =====
    ColorMode      GetColorMode()        const { return colorMode_; }
    void           SetColorMode(ColorMode m);

    const Vector4& GetStartColor()       const { return startColor_; }
    const Vector4& GetEndColor()         const { return endColor_; }
    void           SetStartColor(const Vector4& c) { startColor_ = c; }
    void           SetEndColor(const Vector4& c) { endColor_ = c; }

    CurveProperty& GetRCurve() { return curveR_; }
    CurveProperty& GetGCurve() { return curveG_; }
    CurveProperty& GetBCurve() { return curveB_; }
    CurveProperty& GetACurve() { return curveA_; }

private:
#ifdef USE_IMGUI
    void DrawTwoColorEditor();
    void DrawGradientCurveEditor();
    void RebuildDelegate();

    std::unique_ptr<CurveDelegate> delegate_;
    bool delegateDirty_ = true;
#endif

    ColorMode colorMode_ = ColorMode::TwoColor;
    Vector4   startColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4   endColor_{ 1.0f, 0.0f, 0.0f, 0.0f };

    // GradientCurve モード用の各チャンネル
    CurveProperty curveR_{ 1.0f, "R" };
    CurveProperty curveG_{ 1.0f, "G" };
    CurveProperty curveB_{ 1.0f, "B" };
    CurveProperty curveA_{ 1.0f, "A" };
};

REGISTER_UPDATE_MODULE(UpdateColor)