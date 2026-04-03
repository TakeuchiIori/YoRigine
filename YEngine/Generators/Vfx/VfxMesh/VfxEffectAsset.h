#pragma once
// ===========================================================
// VfxEffectAsset.h
//
// Trail と LightVolume のパラメータをまとめるデータ構造
// JSON にシリアライズして保存・ホットリロードに対応
// ===========================================================
#include "MathFunc.h"
#include <string>
#include "Loaders/Json/EnumUtils.h" // BlendMode

namespace YoRigine {

    // -------------------------------------------------------
    // Trail パラメータ
    // -------------------------------------------------------
    struct TrailEffectParam
    {
        float widthStart = 0.3f;  // 根元の幅
        float widthEnd = 0.0f;  // 先端の幅
        float lifetime = 0.5f;  // トレイル1点の寿命(秒)
        int   maxPoints = 32;    // 最大保持ポイント数

        Vector4 colorStart = { 1.f, 0.8f, 0.f, 1.f }; // 根元カラー(RGBA)
        Vector4 colorEnd = { 1.f, 0.3f, 0.f, 0.f }; // 先端カラー(RGBA)

        BlendMode blendMode = BlendMode::kBlendModeAdd; // ブレンドモード

        float uvScrollSpeed = 0.5f;  // UV スクロール速度
        std::string texturePath = ""; // テクスチャパス (空なら白)
    };

    // -------------------------------------------------------
    // LightVolume パラメータ
    // -------------------------------------------------------
    struct LightVolumeEffectParam
    {
        Vector3 halfExtents = { 2.f, 1.5f, 5.f }; // OBB 半辺長 (X/Y/Z)
        Vector4 color = { 1.f, 0.9f, 0.f, 0.15f }; // RGB + アルファ強度
        float intensity = 1.0f;
        bool  isEnable = true;
    };

    // -------------------------------------------------------
    // まとめアセット（1ファイル = 1エフェクト定義）
    // -------------------------------------------------------
    struct VfxEffectAsset
    {
        std::string       name = "NewEffect";
        TrailEffectParam  trail;
        LightVolumeEffectParam lightVolume;
        bool useTrail = true;
        bool useLightVolume = true;

        // JSON 入出力
        void SaveToJson(const std::string& filePath) const;
        bool LoadFromJson(const std::string& filePath);
    };

} // namespace YoRigine