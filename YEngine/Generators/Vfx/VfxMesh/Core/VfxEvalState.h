#pragma once
// ===========================================================
// VfxEvalState.h
//
// エフェクトの「動き」を Mesh に反映するための共有ランタイム状態。
// モジュール評価側（Spawner / Editor）が毎フレーム値を書き込み、
// 各 ProceduralMesh の Drive() がこれを読んで自分の姿勢に落とす。
// 色・不透明度・表示状態は CB 書き込み時（Draw）に適用する。
//
// これにより「動きの計算」と「Mesh 種類」が分離され、
// 同じ計算を Spawner と Editor で二重に持たなくて済む。
// ===========================================================
#include "MathFunc.h"   // Vector3 / Vector4
#include "Quaternion.h" // Quaternion

namespace YoRigine {

    struct VfxEvalState
    {
        // ── 時間・進捗 ────────────────────────────────────────
        float   age       = 0.f;   ///< 生成からの経過秒
        float   progress  = -1.f;  ///< 0..1=ワンショット進捗 / -1=ループ継続
        float   lifetime  = -1.f;  ///< ワンショット全体寿命(秒) / -1=ループ（FadeInOut の終端計算に使用）

        // ── トランスフォーム ──────────────────────────────────
        Vector3    position = { 0.f, 0.f, 0.f }; ///< 基準位置（モジュールの移動を合成した最終位置）
        float      scale    = 1.f;               ///< 全体スケール
        Quaternion rotation = Quaternion::Identity(); ///< 向き（生成時指定 / 将来の Spin モジュール）

        // ── 方向性エフェクト用（稲妻など） ───────────────────
        Vector3 boltStart = { 0.f, 0.f, 0.f }; ///< 始点ワールド座標
        Vector3 boltEnd   = { 0.f, 0.f, 0.f }; ///< 終点ワールド座標

        // false: LightningMesh は boltStart/boltEnd をそのまま使う
        // true : LightningEffectParam の direction/length から端点を自動計算する
        bool    useAutoEndpoints = true;

        // ── モジュール評価結果（Draw 時に CB の色へ掛ける） ──
        Vector4 colorTint       = { 1.f, 1.f, 1.f, 1.f }; ///< rgb=色乗算(HDR可) / a=不透明度乗算
        float   beamRadiusScale = 1.f;                    ///< LightVolume の beamRadius 乗算
        float   beamGlowScale   = 1.f;                    ///< LightVolume の beamGlow 乗算
        bool    visible         = true;                   ///< Visibility モジュールで false になる
    };

} // namespace YoRigine
