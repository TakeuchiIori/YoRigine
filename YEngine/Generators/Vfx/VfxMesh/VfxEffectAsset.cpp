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
        j["useSmoke"]       = useSmoke;
        j["useLightning"]   = useLightning;

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

        // 3D プリミティブ ★NEW
        {
            auto& pr = t["primitive"];
            const auto& sp = trail.primitive;
            pr["type"]         = static_cast<int>(sp.type);
            pr["placement"]    = static_cast<int>(sp.placement);
            pr["halfExtents"]  = sp.halfExtents;
            pr["radius"]       = sp.radius;
            pr["height"]       = sp.height;
            pr["tubeRadius"]   = sp.tubeRadius;
            pr["latSegments"]  = sp.latSegments;
            pr["lonSegments"]  = sp.lonSegments;
            pr["ringSegments"] = sp.ringSegments;
            pr["stampScale"]   = sp.stampScale;
            pr["stampSpacing"] = sp.stampSpacing;
            pr["scaleByAge"]   = sp.scaleByAge;
        }

        // --- LightVolume ---
        auto& lv = j["lightVolume"];
        lv["halfExtents"] = lightVolume.halfExtents;
        lv["color"]       = lightVolume.color;
        lv["intensity"]   = lightVolume.intensity;
        lv["isEnable"]    = lightVolume.isEnable;

        // --- Volume Smoke ---
        auto& sm = j["smoke"];
        sm["color"]         = smoke.color;
        sm["radius"]        = smoke.radius;
        sm["noiseScale"]    = smoke.noiseScale;
        sm["noiseStrength"] = smoke.noiseStrength;
        sm["scrollSpeed"]   = smoke.scrollSpeed;
        sm["fresnelPower"]  = smoke.fresnelPower;
        sm["density"]       = smoke.density;
        sm["noiseOctaves"]  = smoke.noiseOctaves;
        sm["rimIntensity"]  = smoke.rimIntensity;
        sm["isEnable"]      = smoke.isEnable;

        // --- Lightning ---
        auto& lt = j["lightning"];
        lt["color"]        = lightning.color;
        lt["length"]       = lightning.length;
        lt["width"]        = lightning.width;
        lt["jitter"]       = lightning.jitter;
        lt["segments"]     = lightning.segments;
        lt["branches"]     = lightning.branches;
        lt["branchJitter"] = lightning.branchJitter;
        lt["flickerRate"]  = lightning.flickerRate;
        lt["glowPower"]    = lightning.glowPower;
        lt["isEnable"]     = lightning.isEnable;

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
        useSmoke       = j.value("useSmoke",       useSmoke);
        useLightning   = j.value("useLightning",   useLightning);

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

            // 3D プリミティブ ★NEW (旧 JSON 互換: primitive キーがなければデフォルト)
            if (t.contains("primitive") && t["primitive"].is_object()) {
                const auto& pr = t["primitive"];
                auto& sp = trail.primitive;
                sp.type         = static_cast<PrimitiveType>(pr.value("type",         static_cast<int>(sp.type)));
                sp.placement    = static_cast<PrimitivePlacement>(pr.value("placement", static_cast<int>(sp.placement)));
                if (pr.contains("halfExtents")) sp.halfExtents = pr["halfExtents"];
                sp.radius       = pr.value("radius",       sp.radius);
                sp.height       = pr.value("height",       sp.height);
                sp.tubeRadius   = pr.value("tubeRadius",   sp.tubeRadius);
                sp.latSegments  = pr.value("latSegments",  sp.latSegments);
                sp.lonSegments  = pr.value("lonSegments",  sp.lonSegments);
                sp.ringSegments = pr.value("ringSegments", sp.ringSegments);
                sp.stampScale   = pr.value("stampScale",   sp.stampScale);
                sp.stampSpacing = pr.value("stampSpacing", sp.stampSpacing);
                sp.scaleByAge   = pr.value("scaleByAge",   sp.scaleByAge);
            }
        }

        if (j.contains("lightVolume")) {
            auto& lv = j["lightVolume"];
            lightVolume.intensity = lv.value("intensity", lightVolume.intensity);
            lightVolume.isEnable  = lv.value("isEnable",  lightVolume.isEnable);
            if (lv.contains("halfExtents")) lightVolume.halfExtents = lv["halfExtents"];
            if (lv.contains("color"))       lightVolume.color       = lv["color"];
        }

        if (j.contains("smoke")) {
            auto& sm = j["smoke"];
            if (sm.contains("color")) smoke.color = sm["color"];
            smoke.radius        = sm.value("radius",        smoke.radius);
            smoke.noiseScale    = sm.value("noiseScale",    smoke.noiseScale);
            smoke.noiseStrength = sm.value("noiseStrength", smoke.noiseStrength);
            smoke.scrollSpeed   = sm.value("scrollSpeed",   smoke.scrollSpeed);
            smoke.fresnelPower  = sm.value("fresnelPower",  smoke.fresnelPower);
            smoke.density       = sm.value("density",       smoke.density);
            smoke.noiseOctaves  = sm.value("noiseOctaves",  smoke.noiseOctaves);
            smoke.rimIntensity  = sm.value("rimIntensity",  smoke.rimIntensity);
            smoke.isEnable      = sm.value("isEnable",      smoke.isEnable);
        }

        if (j.contains("lightning")) {
            auto& lt = j["lightning"];
            if (lt.contains("color")) lightning.color = lt["color"];
            lightning.length       = lt.value("length",       lightning.length);
            lightning.width        = lt.value("width",        lightning.width);
            lightning.jitter       = lt.value("jitter",       lightning.jitter);
            lightning.segments     = lt.value("segments",     lightning.segments);
            lightning.branches     = lt.value("branches",     lightning.branches);
            lightning.branchJitter = lt.value("branchJitter", lightning.branchJitter);
            lightning.flickerRate  = lt.value("flickerRate",  lightning.flickerRate);
            lightning.glowPower    = lt.value("glowPower",    lightning.glowPower);
            lightning.isEnable     = lt.value("isEnable",     lightning.isEnable);
        }
        return true;
    }

} // namespace YoRigine
