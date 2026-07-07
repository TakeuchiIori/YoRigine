#pragma once
// ===========================================================
// VfxMotion.h
//
// エフェクトの「動き」をデータとして表すコンポーネント。
// enum + POD struct（ポインタを持たない）で、アセットに複数積める。
// 追加する動きは VfxMotionType に1つ、EvaluateMotions() に1 case
// 足すだけで、Smoke/Lightning/Shockwave など全 Mesh に適用できる。
//
// UE5 Niagara のモジュール（Scale/Color over Life・Fade・Gravity・
// Orbit・遅延ウィンドウ・イージング）を参考にした構成。
// 爆発などのワンショット演出は、これらの組み合わせだけで再現できる。
//
// 「動きの計算」はここ（データ）＋ EvaluateMotions（唯一の評価関数）に集約し、
// Spawner と Editor が同じ関数を呼ぶことで見た目を一致させる。
// ===========================================================
#include <cstdint>
#include "MathFunc.h"   // Vector3 / Vector4

namespace YoRigine {

    // 動きの種類。data-driven に列挙で管理する。
    enum class VfxMotionType : uint8_t
    {
        BurstGrow = 0,  // 爆発ワンショットの寿命を定義（age/duration で 0→1 に進行）
        Move,           // velocity 方向へ基準位置を等速移動
        Rise,           // 上方向へ上昇（velocity.y * amplitude）
        Pulse,          // スケールを sin で脈動（amplitude=振幅 / frequency=Hz）

        // ---- UE5 Niagara 参考のモジュール群 ----
        ScaleOverLife,  // スケールを scaleStart→scaleEnd へ補間（ease 付き）
        ColorOverLife,  // 色乗算を colorStart→colorEnd へ補間（HDR可 / a=不透明度）
        FadeInOut,      // fadeIn 秒でフェードイン、終端 fadeOut 秒でフェードアウト
        Accelerate,     // 初速 velocity + 加速度 acceleration（重力・打ち上げ等）
        Orbit,          // velocity を軸に半径 amplitude で周回（frequency=回転/秒）
        Shake,          // 位置を細かく揺らす（amplitude=振れ幅 / frequency=速さ）
        Visibility,     // startTime〜startTime+window の間だけ表示（演出の順序付け）
        Flicker,        // 不透明度をランダム明滅（amplitude=深さ / frequency=回/秒）
        BeamPulse,      // LightVolume の beamRadius/beamGlow を脈動
    };

    // 補間のイージング（ScaleOverLife / ColorOverLife などで使用）
    enum class VfxEase : uint8_t
    {
        Linear = 0,
        EaseInQuad,     // ゆっくり始まる
        EaseOutQuad,    // ゆっくり終わる
        EaseInOutQuad,  // 両端ゆっくり
        EaseOutCubic,   // 強めに減速
        EaseOutExpo,    // 爆発向き: 一気に広がって減速
        EaseOutBack,    // 少し行き過ぎて戻る（ポップ感）
    };

    // このモーションを適用するサブ効果。All なら全効果に効く。
    // 個別に指定すると「煙だけ上昇 / 衝撃波は固定」のような作り分けができる。
    // （形状インスタンス個別の motions では常にその形状にだけ効く）
    enum class VfxMotionTarget : uint8_t
    {
        All = 0,
        Smoke,
        Lightning,
        Shockwave,
        LightVolume,
    };

    // 1つの動き。用途ごとに使うフィールドが変わる（type で解釈）。
    struct VfxMotion
    {
        VfxMotionType   type      = VfxMotionType::BurstGrow;
        VfxMotionTarget target    = VfxMotionTarget::All;   // 適用対象
        VfxEase         ease      = VfxEase::Linear;        // 補間カーブ

        // ---- タイミング（全タイプ共通。UE5 の Emitter ループ相当） ----
        float startTime = 0.f;   // 開始遅延(秒)。この時刻までモーションは不発
        float window    = 0.f;   // 効果時間(秒)。0=無限（ワンショット時は寿命全体）

        // ---- タイプ別パラメータ ----
        float           duration  = 2.0f;               // BurstGrow: 寿命(秒)
        Vector3         velocity  = { 0.f, 0.f, 0.f };  // Move/Rise/Accelerate: 速度 / Orbit: 回転軸
        Vector3         acceleration = { 0.f, 0.f, 0.f };// Accelerate: 加速度(重力など)
        float           amplitude = 1.0f;               // Pulse/Shake/BeamPulse: 振幅 / Rise: 係数 / Orbit: 半径 / Flicker: 深さ
        float           frequency = 1.0f;               // Pulse/Shake/Orbit/Flicker/BeamPulse: 周波数(Hz)
        float           scaleStart = 0.f;               // ScaleOverLife: 開始スケール倍率
        float           scaleEnd   = 1.f;               // ScaleOverLife: 終了スケール倍率
        Vector4         colorStart = { 1.f, 1.f, 1.f, 1.f }; // ColorOverLife: 開始色乗算(HDR可)
        Vector4         colorEnd   = { 1.f, 1.f, 1.f, 1.f }; // ColorOverLife: 終了色乗算
        float           fadeIn    = 0.1f;               // FadeInOut: フェードイン秒
        float           fadeOut   = 0.3f;               // FadeInOut: フェードアウト秒
    };

} // namespace YoRigine
