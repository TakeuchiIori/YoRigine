#pragma once
// ===========================================================
// VfxEvalState.h
//
// エフェクトの「動き」を Mesh に反映するための共有ランタイム状態。
// モーション評価側（Spawner / Editor）が毎フレーム値を書き込み、
// 各 ProceduralMesh の Drive() がこれを読んで自分の姿勢に落とす。
// 色・不透明度・表示状態は CB 書き込み時（Draw）に適用する。
//
// これにより「動きの計算」と「Mesh 種類」が分離され、
// 同じ計算を Spawner と Editor で二重に持たなくて済む。
// ===========================================================
#include "MathFunc.h"   // Vector3 / Vector4

namespace YoRigine {

    struct VfxEvalState
    {
        float   age       = 0.f;              // 生成からの経過秒
        float   progress  = -1.f;             // 0..1 = ワンショット進捗 / -1 = ループ継続
        float   lifetime  = -1.f;             // ワンショット全体寿命(秒) / -1 = ループ（FadeInOut の終端計算に使用）
        Vector3 position  = { 0.f, 0.f, 0.f }; // 基準位置（＋モーションの移動を合成した最終位置）
        float   scale     = 1.f;              // 全体スケール
        Vector3 boltStart = { 0.f, 0.f, 0.f }; // 方向性エフェクト（稲妻など）の始点
        Vector3 boltEnd   = { 0.f, 0.f, 0.f }; // 同 終点

        // モーションが積み上げる見た目の乗算値（Draw 時に CB の色へ掛ける）
        Vector4 colorTint       = { 1.f, 1.f, 1.f, 1.f }; // rgb=色乗算(HDR可) / a=不透明度乗算
        float   beamRadiusScale = 1.f;                    // LightVolume の beamRadius 乗算
        float   beamGlowScale   = 1.f;                    // LightVolume の beamGlow 乗算
        bool    visible         = true;                   // Visibility モーションで false になる
    };

} // namespace YoRigine
