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
///   EditorConfig を渡すことで数値表示・キー管理UIを呼び出し側から制御できる。
/// </summary>
class CurveProperty {
public:
    enum class Mode : uint8_t {
        Constant,       ///< valMin を固定値として使用
        RandomBetween,  ///< [valMin, valMax] の範囲でランダム
        Curve           ///< channel のカーブに沿った値
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
    void  SetMode(Mode m) { mode_ = m; }
    Mode  GetMode()               const { return mode_; }

    void  SetConstant(float v) { valMin_ = valMax_ = v; mode_ = Mode::Constant; }
    void  SetRandom(float mn, float mx) { valMin_ = mn; valMax_ = mx; mode_ = Mode::RandomBetween; }

    float GetValMin()             const { return valMin_; }
    float GetValMax()             const { return valMax_; }
    void  SetValMin(float v) { valMin_ = v; }
    void  SetValMax(float v) { valMax_ = v; }

    //===== チャンネルアクセス =====
    CurveChannel& GetChannel() { return channel_; }
    const CurveChannel& GetChannel() const { return channel_; }

    //===== JSON =====
    nlohmann::json SaveToJson()                        const;
    void           LoadFromJson(const nlohmann::json& j);

#ifdef USE_IMGUI
    /// <summary>
    /// DrawEditor の表示設定。呼び出し側で自由にカスタマイズして渡す。
    /// </summary>
    struct EditorConfig {
        // ---- 基本 ----
        float    editorHeight = 120.0f;    ///< カーブエディタの高さ (px)
        float    dragSpeed = 0.01f;     ///< DragFloat のスピード
        float    valueMin = 0.0f;      ///< Y軸の表示下限
        float    valueMax = 1.0f;      ///< Y軸の表示上限
        uint32_t curveColor = 0xFF00CCFF;///< カーブ色 (ImGui ABGR)

        // ---- 数値可視化 ----
        bool     showYLabels = true;      ///< Y軸目盛ラベルを表示するか
        int      yLabelCount = 5;         ///< Y軸ラベルの分割数
        bool     showXLabels = false;     ///< X軸(時間)目盛ラベルを表示するか
        int      xLabelCount = 4;         ///< X軸ラベルの分割数
        bool     showKeyValues = true;      ///< 各キーポイントの値をオーバーレイ表示するか

        // ---- キー管理UI ----
        bool     showKeyList = true;      ///< キー一覧 + 追加/削除パネルを表示するか
        int      minKeyCount = 2;         ///< 削除ガード：これ以下では削除ボタンを無効化
    };

    /// <summary>
    /// モードセレクタ + 値入力 / カーブエディタを表示。
    /// </summary>
    /// @param label   ラベル文字列（ImGui::PushID に使用。"#" 始まりならチャンネル名を代用）
    /// @param config  表示設定（省略時はデフォルト値）
    void DrawEditor(const char* label = "##CurveProp",
        const EditorConfig& config = {});
#endif

private:
    Mode         mode_ = Mode::Constant;
    float        valMin_ = 1.0f;
    float        valMax_ = 1.0f;
    CurveChannel channel_;

#ifdef USE_IMGUI
    // キー追加パネルの入力値（インスタンスごとに独立させるためメンバ変数）
    float addKeyTime_ = 0.5f;
    float addKeyValue_ = 1.0f;
#endif
};