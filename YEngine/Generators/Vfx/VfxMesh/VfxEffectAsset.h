#pragma once
// ===========================================================
// VfxEffectAsset.h
//
// Trail と LightVolume のパラメータをまとめるデータ構造
// JSON にシリアライズして保存・ホットリロードに対応
//
// ★ UE Niagara Ribbon Trail 参考の拡張パラメータ追加済み
// ===========================================================
#include "MathFunc.h"
#include <string>
#include "Loaders/Json/EnumUtils.h" // BlendMode

namespace YoRigine {

    // -------------------------------------------------------
    // Trail パラメータ
    // -------------------------------------------------------

    enum class TrailShapeType : int
    {
        Flat      = 0,
        Arc       = 1,
        Fan       = 2,
        Custom    = 3,
        Primitive = 4,   // 3D プリミティブ (Box/Sphere/...) ★NEW
    };

    // 3D プリミティブ種別
    enum class PrimitiveType : int
    {
        Box      = 0,
        Sphere   = 1,
        Capsule  = 2,
        Cone     = 3,
        Cylinder = 4,
        Torus    = 5,
    };

    // プリミティブの配置モード
    enum class PrimitivePlacement : int
    {
        Static          = 0,   // 1個固定 (装飾メッシュ用途)
        BeadAlongTrail  = 1,   // 軌跡の各点にスタンプ (ビーズ状)
    };

    // 3D プリミティブの形状/配置パラメータ
    struct PrimitiveSpec
    {
        PrimitiveType      type      = PrimitiveType::Box;
        PrimitivePlacement placement = PrimitivePlacement::Static;

        // ---- 共通サイズ ----
        Vector3 halfExtents = { 0.5f, 0.5f, 0.5f };  // Box の半辺長
        float   radius      = 0.5f;                   // Sphere/Capsule/Cone/Cylinder/Torus
        float   height      = 1.0f;                   // Capsule/Cone/Cylinder
        float   tubeRadius  = 0.15f;                  // Torus

        // ---- 分割数 ----
        int     latSegments  = 16;   // Sphere/Capsule の縦
        int     lonSegments  = 16;   // Sphere/Capsule/Cone/Cylinder の横
        int     ringSegments = 12;   // Torus の細い側の分割

        // ---- BeadAlongTrail モード用 ----
        float   stampScale   = 1.0f;
        float   stampSpacing = 0.0f;   // 0 = 平滑化後の全点に置く
        bool    scaleByAge   = true;   // 寿命で縮小フェード
    };


    struct TrailEffectParam
    {
        // --- 基本 ---
        float widthStart   = 0.3f;
        float widthEnd     = 0.0f;
        float lifetime     = 0.5f;
        int   maxPoints    = 512;

        Vector4 colorStart = { 1.f, 0.8f, 0.f, 1.f };
        Vector4 colorEnd   = { 1.f, 0.3f, 0.f, 0.f };

        BlendMode blendMode      = BlendMode::kBlendModeAdd;
        float     uvScrollSpeed  = 0.5f;

        std::string texturePath      = "";
        std::string noiseTexturePath = "";

        // --- 断面形状 ---
        TrailShapeType shapeType     = TrailShapeType::Flat;
        int            widthSegments = 1;
        float          arcAngleDeg   = 120.0f;

        bool  crescentShape = true;
        float thickness     = 0.1f;
        std::vector<Vector2> customVertices;
        int splineSubdivisions = 4;

        // ★★ UE Niagara 参考の拡張パラメータ ★★

        // --- グロー / エッジ ---
        float   softness       = 0.15f;    // エッジソフトフェード幅
        float   glowPower      = 1.5f;     // 中心コアグロー強度
        float   fresnelStrength = 1.0f;    // エッジフレネルグロー強度
        float   trailSharpness  = 2.0f;    // エッジシャープネス
        Vector4 rimColor       = { 0.4f, 0.7f, 1.f, 1.f }; // エッジリムカラー

        // --- ノイズ歪み ---
        float distortion    = 0.0f;     // UV 歪み強度 (0=OFF)
        float noiseOctaves  = 2.0f;     // FBM オクターブ数 (1-4)

        // --- エネルギーライン (UE の電気/魔力ライン) ---
        float energyIntensity = 1.5f;   // エネルギーライン輝度
        float energySpeed     = 2.5f;   // エネルギーラインスクロール速度

        // --- スパークル ---
        float sparkleAmount = 0.4f;     // スパークル輝度 (0=OFF)
        float sparkleSpeed  = 4.0f;     // スパークルアニメ速度

        // --- カラーウェーブ ---
        float colorWaveFreq = 3.0f;     // 色波の周波数
        float colorWaveAmp  = 0.15f;    // 色波の振幅 (0=OFF)

        // --- 幅ウェーブ (慣性感) ---
        float widthWaveAmp  = 0.05f;    // 幅のサイン波振幅 (0=OFF)
        float widthWaveFreq = 8.0f;     // 幅のサイン波周波数

        // --- 3D プリミティブ (shapeType == Primitive の時に参照) ★NEW ---
        PrimitiveSpec primitive;
    };

    // -------------------------------------------------------
    // LightVolume パラメータ (変更なし)
    // -------------------------------------------------------
    struct LightVolumeEffectParam
    {
        Vector3 halfExtents  = { 2.f, 1.5f, 5.f };
        Vector4 color        = { 1.f, 0.9f, 0.f, 0.15f };
        float   intensity    = 1.0f;
        bool    isEnable     = true;
    };

    // -------------------------------------------------------
    // Volume Smoke パラメータ (Omen 風めらめらスモーク)
    // -------------------------------------------------------
    struct SmokeEffectParam
    {
        Vector4 color        = { 0.6f, 0.4f, 1.2f, 1.0f }; // rgb(>1でBloom), a=濃度基準
        Vector4 smokeColor   = { 0.08f, 0.08f, 0.09f, 1.0f }; // 爆発後に遷移する煙色(暗いグレー)
        float   riseSpeed    = 1.2f;   // 爆発後に煙が上昇する速さ
        float   radius       = 1.5f;
        float   noiseScale   = 4.0f;
        float   noiseStrength = 0.9f;
        float   scrollSpeed  = 0.3f;
        float   fresnelPower = 2.5f;
        float   density      = 1.0f;
        float   noiseOctaves = 4.0f;
        float   rimIntensity = 2.0f;   // リム発光（太陽フレア風 / Bloom 用）
        bool    isEnable     = true;
    };

    // -------------------------------------------------------
    // Lightning パラメータ (プロシージャル稲妻)
    // -------------------------------------------------------
    struct LightningEffectParam
    {
        Vector4 color        = { 0.6f, 0.8f, 1.5f, 1.0f }; // 青白（HDR）
        float   length       = 4.0f;   // 稲妻の全長
        float   width        = 0.18f;  // リボン幅
        float   jitter       = 0.6f;   // ジグザグの振れ幅
        int     segments     = 24;     // 折れ線の分割数
        int     branches     = 3;      // 枝分かれ本数
        float   branchJitter = 0.5f;   // 枝の振れ幅
        float   flickerRate  = 18.0f;  // 1秒あたりの形変化回数（明滅）
        float   glowPower    = 2.0f;   // 中心グロー
        bool    isEnable     = true;
    };

    // -------------------------------------------------------
    // Shockwave パラメータ (爆発の衝撃波リング)
    // -------------------------------------------------------
    struct ShockwaveEffectParam
    {
        Vector4 color     = { 1.5f, 0.9f, 0.4f, 1.0f }; // 暖色 HDR
        float   radius    = 3.0f;   // 最大半径
        float   duration  = 0.6f;   // 1サイクルの秒数（膨張→消滅）
        float   thickness = 0.15f;  // リングの太さ
        bool    isEnable  = true;
    };

    // -------------------------------------------------------
    // まとめアセット
    // -------------------------------------------------------
    struct VfxEffectAsset
    {
        std::string       name = "NewEffect";
        TrailEffectParam  trail;
        LightVolumeEffectParam lightVolume;
        SmokeEffectParam  smoke;
        LightningEffectParam lightning;
        ShockwaveEffectParam shockwave;
        bool useTrail       = true;
        bool useLightVolume = true;
        bool useSmoke       = false;   // 既定OFF（既存アセットは従来通り）
        bool useLightning   = false;   // 既定OFF
        bool useShockwave   = false;   // 既定OFF

        void SaveToJson(const std::string& filePath) const;
        bool LoadFromJson(const std::string& filePath);
    };

} // namespace YoRigine
