#pragma once
// ===========================================================
// VfxParamsJson.h
//
// Geometry / Material のパラメータ構造体と variant の
// JSON シリアライズ定義を1か所に集約するヘッダ。
//
// - 数学型 (Vector3/4 / Quaternion) と std::variant の変換は
//   Loaders/Json/ConversionJson.h が提供する。
// - 各パラメータ構造体は NLOHMANN_DEFINE_TYPE_..._WITH_DEFAULT で対応。
//   WITH_DEFAULT なので後からフィールドを追加しても旧 JSON が壊れない。
//
// このヘッダを include したところで AutoJson / nlohmann の
//   json j = geomParams;   /   params = j.get<VfxGeometryParams>();
// が使えるようになる。
// ===========================================================
#include <json.hpp>
#include "Loaders/Json/ConversionJson.h" // Vector/Quaternion/variant の to_json/from_json
#include "VfxGeometryParams.h"
#include "VfxMaterialParams.h"

namespace YoRigine {

    // ── ジオメトリパラメータ ─────────────────────────────────
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        SphereGeomParams, radius, rings, sectors)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        ConeGeomParams, radius, height, segments)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        RingGeomParams, outerRadius, innerRatio, segments, billboard)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        PlaneGeomParams, size, billboard)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        DiscGeomParams, radius, segments, billboard, pitchDeg, yawDeg)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        RimCurtainGeomParams, radius, heightRatio, segments)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        LobeClusterGeomParams, radius, lobeCount, lobeRadius, lobeJitter, stagger,
        rings, sectors, seed)

    // ── マテリアルパラメータ ─────────────────────────────────
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        NoiseMatParams, color, smokeColor, radius, noiseScale, noiseStrength,
        scrollSpeed, fresnelPower, density, noiseOctaves, rimIntensity, builtInBurstMotion)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        ShockwaveMatParams, color, thickness, ringRadius)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        AreaFieldMatParams, color, edgeColor, fillOpacity, edgeWidth, ringSpeed,
        noiseScale, scanSpeed, runeCount, style, fillStyle, styleRune, styleEnergy, styleScan)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        RimFxMatParams, color, tipColor, style, riseSpeed, noiseScale, turbulence,
        texturePath, texStrength, texTiling, texScroll)

} // namespace YoRigine
