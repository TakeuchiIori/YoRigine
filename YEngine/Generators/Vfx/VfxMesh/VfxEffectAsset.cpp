// ===========================================================
// VfxEffectAsset.cpp
// nlohmann/json を使用 (既存のJsonローダーに合わせて変更可)
// ===========================================================
#include "VfxEffectAsset.h"
#include <fstream>
#include <json.hpp>
#include "Debugger/Logger.h"
#include "Loaders/Json/JsonConverters.h"
using json = nlohmann::json;

namespace YoRigine {

    // -------------------------------------------------------
    void VfxEffectAsset::SaveToJson(const std::string& filePath) const {
        json j;
        j["name"] = name;
        j["useTrail"] = useTrail;
        j["useLightVolume"] = useLightVolume;

        // --- Trail ---
        auto& t = j["trail"];
        t["widthStart"] = trail.widthStart;
        t["widthEnd"] = trail.widthEnd;
        t["lifetime"] = trail.lifetime;
        t["maxPoints"] = trail.maxPoints;
        t["colorStart"] = Vector4ToJson(trail.colorStart);
        t["colorEnd"] = Vector4ToJson(trail.colorEnd);
        t["blendMode"] = static_cast<int>(trail.blendMode);
        t["uvScrollSpeed"] = trail.uvScrollSpeed;
        t["texturePath"] = trail.texturePath;
        t["noiseTexturePath"] = trail.noiseTexturePath;

        // --- LightVolume ---
        auto& lv = j["lightVolume"];
        lv["halfExtents"] = Vector3ToJson(lightVolume.halfExtents);
        lv["color"] = Vector4ToJson(lightVolume.color);
        lv["intensity"] = lightVolume.intensity;
        lv["isEnable"] = lightVolume.isEnable;

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

        name = j.value("name", name);
        useTrail = j.value("useTrail", useTrail);
        useLightVolume = j.value("useLightVolume", useLightVolume);

        if (j.contains("trail")) {
            auto& t = j["trail"];
            trail.widthStart = t.value("widthStart", trail.widthStart);
            trail.widthEnd = t.value("widthEnd", trail.widthEnd);
            trail.lifetime = t.value("lifetime", trail.lifetime);
            trail.maxPoints = t.value("maxPoints", trail.maxPoints);
            trail.uvScrollSpeed = t.value("uvScrollSpeed", trail.uvScrollSpeed);
            trail.texturePath = t.value("texturePath", trail.texturePath);
            trail.noiseTexturePath = t.value("noiseTexturePath", trail.noiseTexturePath);
            trail.blendMode = static_cast<BlendMode>(t.value("blendMode", static_cast<int>(trail.blendMode)));
            if (t.contains("colorStart")) trail.colorStart = JsonToVector4(t["colorStart"]);
            if (t.contains("colorEnd"))   trail.colorEnd = JsonToVector4(t["colorEnd"]);
        }
        if (j.contains("lightVolume")) {
            auto& lv = j["lightVolume"];
            lightVolume.intensity = lv.value("intensity", lightVolume.intensity);
            lightVolume.isEnable = lv.value("isEnable", lightVolume.isEnable);
            if (lv.contains("halfExtents")) lightVolume.halfExtents = JsonToVector3(lv["halfExtents"]);
            if (lv.contains("color"))       lightVolume.color = JsonToVector4(lv["color"]);
        }
        return true;
    }

} // namespace YoRigine