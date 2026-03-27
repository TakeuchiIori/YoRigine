#pragma once
#include "CurveKey.h"
#include <vector>
#include <string>
#include <json.hpp>

/// <summary>
/// 1 チャンネル分のフロートカーブ。
/// 複数の CurveKey を保持し、任意の時刻 t に対してサンプリングできる。
/// ImGui に非依存 — リリースビルドでもそのまま使用可。
///
/// 補間モードはキー単位で指定できる（あるキーから次のキーへの補間方法）。
/// SetDefaultMode() でまとめて変更する場合は以降に追加するキーに適用される。
/// </summary>
class CurveChannel {
public:
    //===== 構築 =====
    CurveChannel();
    explicit CurveChannel(const std::string& name,
                          float viewMin = 0.0f,
                          float viewMax = 1.0f);

    //===== キー操作 =====
    void AddKey(float time, float value,
                InterpolationMode mode = InterpolationMode::Linear,
                float inTangent  = 0.0f,
                float outTangent = 0.0f);

    void RemoveKey(int index);
    void MoveKey(int index, float newTime, float newValue);
    void SetKeyTangents(int index, float inT, float outT);
    void SetKeyMode(int index, InterpolationMode mode);
    void SortByTime();
    void Clear();

    /// デフォルト値で 2 点（t=0, t=1）にリセット
    void Reset(float constValue = 1.0f);

    //===== 評価 =====
    /// t を keys の範囲内にクランプして評価
    float Evaluate(float t) const;

    /// 範囲外は端点の値を返す（クランプせず外挿しない）
    float EvaluateUnclamped(float t) const;

    //===== アクセス =====
    int                          GetKeyCount() const { return (int)keys_.size(); }
    const CurveKey&              GetKey(int i) const { return keys_[i]; }
    CurveKey&                    GetKey(int i)       { return keys_[i]; }
    const std::vector<CurveKey>& GetKeys()     const { return keys_; }
    std::vector<CurveKey>&       GetKeys()           { return keys_; }

    const std::string& GetName()   const               { return name_; }
    void               SetName(const std::string& n)   { name_ = n; }

    /// ImCurveEdit 表示域（CurveDelegate が参照する）
    float GetViewMin() const { return viewMin_; }
    float GetViewMax() const { return viewMax_; }
    void  SetViewRange(float mn, float mx) { viewMin_ = mn; viewMax_ = mx; }

    InterpolationMode GetDefaultMode() const           { return defaultMode_; }
    void              SetDefaultMode(InterpolationMode m){ defaultMode_ = m; }

    //===== シリアライズ =====
    nlohmann::json SaveToJson()                       const;
    void           LoadFromJson(const nlohmann::json& j);

private:
    //--- 内部評価 ---
    float EvaluateAt(float t)                          const;
    float EvaluateSegment(int i, float localT)         const;
    float CatmullRomInterp(float p0, float p1,
                           float p2, float p3,
                           float t)                    const;
    float CubicBezierInterp(float v0, float cp1,
                            float cp2, float v1,
                            float t)                   const;

    std::string           name_;
    std::vector<CurveKey> keys_;
    InterpolationMode     defaultMode_ = InterpolationMode::Linear;
    float                 viewMin_     = 0.0f;
    float                 viewMax_     = 1.0f;
};
