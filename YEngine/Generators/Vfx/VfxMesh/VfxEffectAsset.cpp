// ===========================================================
// VfxEffectAsset.cpp
// ===========================================================
#include "VfxEffectAsset.h"
#include <fstream>
#include <json.hpp>
#include "Debugger/Logger.h"
#include "Loaders/Json/ConversionJson.h"
using json = nlohmann::json;

namespace YoRigine {

    // -------------------------------------------------------
    void VfxEffectAsset::SaveToJson(const std::string& filePath) const {
        json j;
        j["name"]           = name;
        j["useTrail"]       = useTrail;
        j["useLightVolume"] = useLightVolume;

        // --- Trail ---
        auto& t = j["trail"];
        // 基本
        t["widthStart"]        = trail.widthStart;
        t["widthEnd"]          = trail.widthEnd;
        t["lifetime"]          = trail.lifetime;
        t["maxPoints"]         = trail.maxPoints;
        t["colorStart"]        = trail.colorStart;
        t["colorEnd"]          = trail.colorEnd;
        t["blendMode"]         = static_cast<int>(trail.blendMode);
        t["uvScrollSpeed"]     = trail.uvScrollSpeed;
        t["texturePath"]       = trail.texturePath;
        t["noiseTexturePath"]  = trail.noiseTexturePath;
        t["shapeType"]         = static_cast<int>(trail.shapeType);
        t["widthSegments"]     = trail.widthSegments;
        t["arcAngleDeg"]       = trail.arcAngleDeg;
        t["crescentShape"]     = trail.crescentShape;
        t["thickness"]         = trail.thickness;
        t["splineSubdivisions"] = trail.splineSubdivisions;
        for (const auto& v : trail.customVertices) {
            t["customVertices"].push_back({ v.x, v.y });
        }

        // ★★ 拡張パラメータ ★★
        // グロー / エッジ
        t["softness"]        = trail.softness;
        t["glowPower"]       = trail.glowPower;
        t["fresnelStrength"] = trail.fresnelStrength;
        t["trailSharpness"]  = trail.trailSharpness;
        t["rimColor"]        = trail.rimColor;

        // ノイズ歪み
        t["distortion"]   = trail.distortion;
        t["noiseOctaves"] = trail.noiseOctaves;

        // エネルギーライン
        t["energyIntensity"] = trail.energyIntensity;
        t["energySpeed"]     = trail.energySpeed;

        // スパークル
        t["sparkleAmount"] = trail.sparkleAmount;
        t["sparkleSpeed"]  = trail.sparkleSpeed;

        // カラーウェーブ
        t["colorWaveFreq"] = trail.colorWaveFreq;
        t["colorWaveAmp"]  = trail.colorWaveAmp;

        // 幅ウェーブ
        t["widthWaveAmp"]  = trail.widthWaveAmp;
        t["widthWaveFreq"] = trail.widthWaveFreq;

        // --- LightVolume ---
        auto& lv = j["lightVolume"];
        lv["halfExtents"] = lightVolume.halfExtents;
        lv["color"]       = lightVolume.color;
        lv["intensity"]   = lightVolume.intensity;
        lv["isEnable"]    = lightVolume.isEnable;

        std::ofstream ofs(filePath);
        ofs << j.dump(4);
    }

    // -------------------------------------------------------
    bool VfxEffectAsset::LoadFromJson(const std::string& filePath) {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) {
            Logger("VfxEffectAsset: file not found: " + filePath);
            return false;
        }
        json j;
        try { ifs >> j; }
        catch (...) { Logger("VfxEffectAsset: JSON parse error: " + filePath); return false; }

        name           = j.value("name",           name);
        useTrail       = j.value("useTrail",       useTrail);
        useLightVolume = j.value("useLightVolume", useLightVolume);

        if (j.contains("trail")) {
            auto& t = j["trail"];

            // 基本
            trail.widthStart       = t.value("widthStart",       trail.widthStart);
            trail.widthEnd         = t.value("widthEnd",         trail.widthEnd);
            trail.lifetime         = t.value("lifetime",         trail.lifetime);
            trail.maxPoints        = t.value("maxPoints",        trail.maxPoints);
            trail.uvScrollSpeed    = t.value("uvScrollSpeed",    trail.uvScrollSpeed);
            trail.texturePath      = t.value("texturePath",      trail.texturePath);
            trail.noiseTexturePath = t.value("noiseTexturePath", trail.noiseTexturePath);
            trail.blendMode        = static_cast<BlendMode>(t.value("blendMode", static_cast<int>(trail.blendMode)));
            trail.shapeType        = static_cast<TrailShapeType>(t.value("shapeType", static_cast<int>(trail.shapeType)));
            trail.widthSegments    = t.value("widthSegments",    trail.widthSegments);
            trail.arcAngleDeg      = t.value("arcAngleDeg",      trail.arcAngleDeg);
            trail.crescentShape    = t.value("crescentShape",    trail.crescentShape);
            trail.thickness        = t.value("thickness",        trail.thickness);
            trail.splineSubdivisions = t.value("splineSubdivisions", trail.splineSubdivisions);

            if (t.contains("colorStart")) trail.colorStart = t["colorStart"];
            if (t.contains("colorEnd"))   trail.colorEnd   = t["colorEnd"];

            if (t.contains("customVertices")) {
                trail.customVertices.clear();
                for (const auto& v : t["customVertices"]) {
                    trail.customVertices.push_back({ v[0].get<float>(), v[1].get<float>() });
                }
            }

            // ★★ 拡張パラメータ (既存 JSON との後方互換: value() でデフォルト値を持つ) ★★
            // グロー / エッジ
            trail.softness        = t.value("softness",        trail.softness);
            trail.glowPower       = t.value("glowPower",       trail.glowPower);
            trail.fresnelStrength = t.value("fresnelStrength", trail.fresnelStrength);
            trail.trailSharpness  = t.value("trailSharpness",  trail.trailSharpness);
            if (t.contains("rimColor")) trail.rimColor = t["rimColor"];

            // ノイズ歪み
            trail.distortion  = t.value("distortion",  trail.distortion);
            trail.noiseOctaves = t.value("noiseOctaves", trail.noiseOctaves);

            // エネルギーライン
            trail.energyIntensity = t.value("energyIntensity", trail.energyIntensity);
            trail.energySpeed     = t.value("energySpeed",     trail.energySpeed);

            // スパークル
            trail.sparkleAmount = t.value("sparkleAmount", trail.sparkleAmount);
            trail.sparkleSpeed  = t.value("sparkleSpeed",  trail.sparkleSpeed);

            // カラーウェーブ
            trail.colorWaveFreq = t.value("colorWaveFreq", trail.colorWaveFreq);
            trail.colorWaveAmp  = t.value("colorWaveAmp",  trail.colorWaveAmp);

            // 幅ウェーブ
            trail.widthWaveAmp  = t.value("widthWaveAmp",  trail.widthWaveAmp);
            trail.widthWaveFreq = t.value("widthWaveFreq", trail.widthWaveFreq);
        }

        if (j.contains("lightVolume")) {
            auto& lv = j["lightVolume"];
            lightVolume.intensity = lv.value("intensity", lightVolume.intensity);
            lightVolume.isEnable  = lv.value("isEnable",  lightVolume.isEnable);
            if (lv.contains("halfExtents")) lightVolume.halfExtents = lv["halfExtents"];
            if (lv.contains("color"))       lightVolume.color       = lv["color"];
        }
        return true;
    }

} // namespace YoRigine
