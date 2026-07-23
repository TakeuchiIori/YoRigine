#pragma once
// ===========================================================
// VfxMaterialParams.h
//
// マテリアルの型 ID とパラメータ（variant）だけを持つ軽量ヘッダ。
// VfxMaterial に依存しないので VfxEffectAsset から安全に include できる。
// ===========================================================
#include <variant>
#include <string>
#include "MathFunc.h"

namespace YoRigine {

    // ── 型 ID ─────────────────────────────────────────────────
    enum class VfxMaterialType : int
    {
        Noise = 0,
        Shockwave,
        AreaField,  // 地面円フィールド（魔法陣/エネルギー/スキャンを1枚で内包）
        RimFx,      // 縁の縦演出（炎/霊気/電撃…を style で切替。RimCurtain と組む）
    };

    // ── 各マテリアルのパラメータ（variant の要素） ────────────
    struct NoiseMatParams
    {
        Vector4 color         = { 0.6f, 0.4f, 1.2f, 1.0f }; ///< rgb>1 で Bloom
        Vector4 smokeColor    = { 0.08f, 0.08f, 0.09f, 1.0f }; ///< バースト後の煙色
        float   radius        = 1.5f; ///< フレネル基準半径（実半径 = radius * element scale）
        float   noiseScale    = 4.0f;
        float   noiseStrength = 0.9f;
        float   scrollSpeed   = 0.3f;
        float   fresnelPower  = 2.5f;
        float   density       = 1.0f;
        float   noiseOctaves  = 4.0f;
        float   rimIntensity  = 2.0f;
        bool    builtInBurstMotion = true;
    };

    struct ShockwaveMatParams
    {
        Vector4 color      = { 1.5f, 0.9f, 0.4f, 1.0f }; ///< HDR 暖色 / a=不透明度
        float   thickness  = 0.15f;  ///< リング太さ(UV)
        float   ringRadius = 0.8f;   ///< クワッドUV内のリング位置(0..1)。膨張は ScaleOverLife モジュールで
    };

    // 地面円フィールド。3スタイル（魔法陣/エネルギー/スキャン）をブレンド係数で内包し、
    // 1マテリアルで大量のバフ/AoE/射程表現を量産する。膨張は element scale が担当。
    struct AreaFieldMatParams
    {
        Vector4 color       = { 0.30f, 1.40f, 0.60f, 1.0f }; ///< HDR ベース色 / a=不透明度
        Vector4 edgeColor   = { 0.80f, 2.40f, 1.20f, 1.0f }; ///< 外周リムの HDR 色
        float   fillOpacity = 0.35f;  ///< 内部塗りの濃さ
        float   edgeWidth   = 0.06f;  ///< 外周リムの帯幅(UV, 0..0.5)
        float   ringSpeed   = 0.50f;  ///< 魔法陣の回転速度(周/秒相当)
        float   noiseScale  = 3.0f;   ///< エネルギーノイズのタイリング
        float   scanSpeed   = 1.20f;  ///< スキャン波の速さ
        float   runeCount   = 6.0f;   ///< 魔法陣の対称数（花弁/ルーン数/多角形の辺）
        int     style       = 0;      ///< 魔法陣デザイン 0=古典/1=同心円/2=時計/3=六芒星/4=五角星/5=花弁/6=レーダー/7=秘術
        int     fillStyle   = 0;      ///< 内部の質感 0=無地/1=毒/2=バフ/3=エネルギー/4=波紋/5=残り火
        float   styleRune   = 1.0f;   ///< 魔法陣デザインの強さ(0..1)
        float   styleEnergy = 0.6f;   ///< ②エネルギー場ブレンド(0..1)
        float   styleScan   = 0.5f;   ///< ③スキャン波ブレンド(0..1)
    };

    // 縁の縦演出。style で表現を切替（0=炎 / 1=霊気 / 2=電撃 / …最大8種）。
    // RimCurtain（縦スカート）と組み、v(下0→上1)に沿って上へ立ち上がる表現を作る。
    struct RimFxMatParams
    {
        Vector4 color      = { 2.5f, 0.8f,  0.15f, 1.0f }; ///< 根本の熱い色(HDR) / a=強度
        Vector4 tipColor   = { 2.2f, 0.20f, 0.04f, 1.0f }; ///< 先端(上)の色(HDR)
        int     style      = 0;     ///< 0=Flame / 1=Wisp / 2=Electric / 3=Petal / 4=Aura / 5=Poison / 6=Vortex / 7=Sparkle
        float   riseSpeed  = 1.2f;  ///< 上昇スクロール速度
        float   noiseScale = 3.0f;  ///< 揺らぎ/炎舌のタイリング（電撃/渦は本数）
        float   turbulence = 1.0f;  ///< 揺らぎの強さ

        // テクスチャ（任意）。強度に乗せるマスク/模様として使う。空なら白テクスチャ扱いで無効。
        std::string texturePath = "";    ///< "Resources/Textures/xxx.png"（リポジトリルート相対）
        float   texStrength = 0.0f;      ///< 0=プロシージャルのみ / 1=テクスチャで完全マスク
        float   texTiling   = 1.0f;      ///< UV タイリング
        float   texScroll   = 0.5f;      ///< テクスチャの縦スクロール速度
    };

    /// マテリアルパラメータの型安全な union（要素順は VfxMaterialType と一致させること）
    using VfxMaterialParams = std::variant<
        NoiseMatParams, ShockwaveMatParams, AreaFieldMatParams, RimFxMatParams>;

} // namespace YoRigine
