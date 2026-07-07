// ===========================================================
// VfxEffectAsset.cpp
// ===========================================================
#include "VfxEffectAsset.h"
#include "VfxEvalState.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <json.hpp>
#include "Debugger/Logger.h"
#include "Loaders/Json/ConversionJson.h"
using json = nlohmann::json;

namespace YoRigine {

    // -------------------------------------------------------
    const char* VfxSubEffectTypeName(VfxSubEffectType type) {
        switch (type) {
        case VfxSubEffectType::LightVolume: return "LightVolume";
        case VfxSubEffectType::Smoke:       return "Smoke";
        case VfxSubEffectType::Lightning:   return "Lightning";
        case VfxSubEffectType::Shockwave:   return "Shockwave";
        }
        return "Unknown";
    }

    VfxMotionTarget MotionTargetFor(VfxSubEffectType type) {
        switch (type) {
        case VfxSubEffectType::LightVolume: return VfxMotionTarget::LightVolume;
        case VfxSubEffectType::Smoke:       return VfxMotionTarget::Smoke;
        case VfxSubEffectType::Lightning:   return VfxMotionTarget::Lightning;
        case VfxSubEffectType::Shockwave:   return VfxMotionTarget::Shockwave;
        }
        return VfxMotionTarget::All;
    }

    // -------------------------------------------------------
    // ワンショット寿命: BurstGrow モーション（全体/形状個別）優先、
    // 無ければ従来ヒューリスティック
    float VfxEffectAsset::OneShotDuration() const {
        float burst = 0.f;
        auto scan = [&burst](const std::vector<VfxMotion>& ms) {
            for (const auto& m : ms) {
                if (m.type == VfxMotionType::BurstGrow) burst = std::max(burst, m.duration);
            }
        };
        scan(motions);
        for (const auto& sub : subEffects) scan(sub.motions);
        if (burst > 0.f) return std::max(burst, 0.01f);

        // ヒューリスティック: 煙があれば最低2秒、衝撃波はその膨張時間
        bool  hasSmoke = false;
        float swDur    = 0.f;
        for (const auto& sub : subEffects) {
            if (!sub.enabled) continue;
            if (sub.type == VfxSubEffectType::Smoke)     hasSmoke = true;
            if (sub.type == VfxSubEffectType::Shockwave) swDur = std::max(swDur, sub.shockwave.duration);
        }
        return std::max(hasSmoke
            ? (swDur > 0.f ? std::max(swDur, 2.0f) : 2.0f)
            : swDur, 0.1f);
    }

    // -------------------------------------------------------
    // イージング（ScaleOverLife / ColorOverLife の補間カーブ）
    static float ApplyEase(VfxEase e, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (e) {
        case VfxEase::EaseInQuad:    return t * t;
        case VfxEase::EaseOutQuad:   return 1.0f - (1.0f - t) * (1.0f - t);
        case VfxEase::EaseInOutQuad: return (t < 0.5f) ? 2.0f * t * t
                                                       : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case VfxEase::EaseOutCubic: {
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        case VfxEase::EaseOutExpo:   return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
        case VfxEase::EaseOutBack: {
            const float c1 = 1.70158f, c3 = c1 + 1.0f;
            const float u = t - 1.0f;
            return 1.0f + c3 * u * u * u + c1 * u * u;
        }
        case VfxEase::Linear:
        default:                     return t;
        }
    }

    // 決定的な擬似乱数（Flicker 用）。Editor / Spawner で同じ明滅になる。
    static float StepNoise(float t) {
        const float x = std::sin(std::floor(t) * 12.9898f) * 43758.5453f;
        return x - std::floor(x); // 0..1
    }

    // -------------------------------------------------------
    // モーション評価: target（または All）に一致する動きを共有状態へ加算する。
    // 位置移動は position だけでなく端点(boltStart/End)にも同じオフセットを掛け、
    // Lightning（端点駆動）も一緒に動くようにする。
    //
    // 全モーション共通のタイミング:
    //   startTime … この時刻まで不発（Visibility だけは「表示ウィンドウ」として解釈）
    //   window    … 効果時間。移動系はこの時間で停止、補間系はこの時間で 0→1。
    //               0 の場合は無限（補間系はワンショット寿命 lifetime/progress を使う）
    // （寿命=progress は Spawner が OneShotDuration() で算出済み。BurstGrow はここでは何もしない）
    void EvaluateMotionList(const std::vector<VfxMotion>& motions,
                            VfxMotionTarget target, VfxEvalState& s) {
        for (const auto& m : motions) {
            if (m.target != VfxMotionTarget::All && m.target != target) continue;

            // ---- Visibility: 表示ウィンドウ外なら非表示にするだけ ----
            if (m.type == VfxMotionType::Visibility) {
                const bool inWindow = (s.age >= m.startTime) &&
                    (m.window <= 0.f || s.age <= m.startTime + m.window);
                if (!inWindow) s.visible = false;
                continue;
            }

            if (m.type == VfxMotionType::BurstGrow) continue; // 寿命定義のみ（ここでは何もしない）

            const float localAge = s.age - m.startTime;
            if (localAge < 0.f) continue; // まだ始まっていない

            // 補間系の正規化時間 0..1（window 優先 → ワンショット寿命 → 1固定）
            float t01;
            if (m.window > 0.f)          t01 = std::clamp(localAge / m.window, 0.0f, 1.0f);
            else if (s.progress >= 0.f)  t01 = s.progress;
            else if (s.lifetime > 0.f)   t01 = std::clamp(s.age / s.lifetime, 0.0f, 1.0f);
            else                         t01 = 1.0f;
            const float eased = ApplyEase(m.ease, t01);

            // 移動系の実効時間（window があればそこで停止）
            const float te = (m.window > 0.f) ? std::min(localAge, m.window) : localAge;

            // 位置と端点を同時に動かすヘルパ
            auto shift = [&s](float dx, float dy, float dz) {
                s.position.x += dx;  s.position.y += dy;  s.position.z += dz;
                s.boltStart.x += dx; s.boltStart.y += dy; s.boltStart.z += dz;
                s.boltEnd.x += dx;   s.boltEnd.y += dy;   s.boltEnd.z += dz;
            };

            switch (m.type) {
            case VfxMotionType::Move:
                shift(m.velocity.x * te, m.velocity.y * te, m.velocity.z * te);
                break;

            case VfxMotionType::Rise:
                shift(0.f, m.velocity.y * m.amplitude * te, 0.f);
                break;

            case VfxMotionType::Pulse:
                s.scale *= 1.0f + m.amplitude * std::sin(s.age * m.frequency * 6.2831853f);
                break;

            case VfxMotionType::ScaleOverLife:
                s.scale *= m.scaleStart + (m.scaleEnd - m.scaleStart) * eased;
                break;

            case VfxMotionType::ColorOverLife:
                s.colorTint.x *= m.colorStart.x + (m.colorEnd.x - m.colorStart.x) * eased;
                s.colorTint.y *= m.colorStart.y + (m.colorEnd.y - m.colorStart.y) * eased;
                s.colorTint.z *= m.colorStart.z + (m.colorEnd.z - m.colorStart.z) * eased;
                s.colorTint.w *= m.colorStart.w + (m.colorEnd.w - m.colorStart.w) * eased;
                break;

            case VfxMotionType::FadeInOut: {
                float alpha = 1.0f;
                if (m.fadeIn > 0.f) alpha *= std::clamp(localAge / m.fadeIn, 0.0f, 1.0f);
                // フェードアウトの終端: window 優先、無ければワンショット寿命
                float end = (m.window > 0.f) ? m.window
                          : (s.lifetime > 0.f ? s.lifetime - m.startTime : -1.f);
                if (end > 0.f && m.fadeOut > 0.f)
                    alpha *= std::clamp((end - localAge) / m.fadeOut, 0.0f, 1.0f);
                s.colorTint.w *= alpha;
                break;
            }

            case VfxMotionType::Accelerate:
                // x(t) = v0*t + 0.5*a*t^2 （重力・打ち上げ・吹き飛ばし）
                shift(m.velocity.x * te + 0.5f * m.acceleration.x * te * te,
                      m.velocity.y * te + 0.5f * m.acceleration.y * te * te,
                      m.velocity.z * te + 0.5f * m.acceleration.z * te * te);
                break;

            case VfxMotionType::Orbit: {
                // velocity を回転軸、amplitude を半径として周回する
                Vector3 axis = m.velocity;
                const float al = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
                axis = (al < 1e-4f) ? Vector3{ 0.f, 1.f, 0.f } : axis / al;
                // 軸に直交する基底 (u, v) を作る
                Vector3 up = (std::fabs(axis.y) < 0.99f) ? Vector3{ 0.f, 1.f, 0.f }
                                                         : Vector3{ 1.f, 0.f, 0.f };
                Vector3 u = { axis.y * up.z - axis.z * up.y,
                              axis.z * up.x - axis.x * up.z,
                              axis.x * up.y - axis.y * up.x };
                const float ul = std::sqrt(u.x * u.x + u.y * u.y + u.z * u.z);
                if (ul > 1e-4f) u = u / ul;
                Vector3 v = { axis.y * u.z - axis.z * u.y,
                              axis.z * u.x - axis.x * u.z,
                              axis.x * u.y - axis.y * u.x };
                const float ang = te * m.frequency * 6.2831853f;
                const float c = std::cos(ang), sn = std::sin(ang);
                shift((u.x * c + v.x * sn) * m.amplitude,
                      (u.y * c + v.y * sn) * m.amplitude,
                      (u.z * c + v.z * sn) * m.amplitude);
                break;
            }

            case VfxMotionType::Shake: {
                // 位相をずらした sin の合成で疑似ランダムに揺らす（決定的）
                const float f = m.frequency * 6.2831853f;
                shift(std::sin(s.age * f + 12.9f) * std::sin(s.age * f * 1.31f + 4.7f) * m.amplitude,
                      std::sin(s.age * f * 0.87f + 78.2f) * std::sin(s.age * f * 1.17f + 1.3f) * m.amplitude,
                      std::sin(s.age * f * 1.13f + 37.7f) * std::sin(s.age * f * 0.73f + 9.1f) * m.amplitude);
                break;
            }

            case VfxMotionType::Flicker: {
                // frequency 回/秒で段階的に変わるランダム明滅
                const float n = StepNoise(s.age * std::max(m.frequency, 0.01f));
                s.colorTint.w *= std::max(0.0f, 1.0f - m.amplitude * n);
                break;
            }

            default:
                break;
            }
        }
    }

    void EvaluateMotions(const VfxEffectAsset& a, VfxMotionTarget target, VfxEvalState& s) {
        EvaluateMotionList(a.motions, target, s);
    }

    void EvaluateSubEffectMotions(const VfxEffectAsset& a,
                                  const VfxSubEffect& sub, VfxEvalState& s) {
        const VfxMotionTarget target = MotionTargetFor(sub.type);
        EvaluateMotionList(a.motions, target, s);   // 全体モーション（対象一致分）
        EvaluateMotionList(sub.motions, target, s); // この形状専用のモーション
    }

    // ===========================================================
    // JSON ヘルパ（サブ効果パラメータの読み書き）
    // ===========================================================
    namespace {

        void ToJson(json& j, const LightVolumeEffectParam& p) {
            j["halfExtents"]   = p.halfExtents;
            j["color"]         = p.color;
            j["intensity"]     = p.intensity;
            j["edgeFade"]      = p.edgeFade;
            j["depthFade"]     = p.depthFade;
            j["noiseTiling"]   = p.noiseTiling;
            j["noiseStrength"] = p.noiseStrength;
            j["beamStrength"]  = p.beamStrength;
            j["beamRadius"]    = p.beamRadius;
            j["beamPower"]     = p.beamPower;
            j["beamGlow"]      = p.beamGlow;
            j["isEnable"]      = p.isEnable;
        }
        void FromJson(const json& j, LightVolumeEffectParam& p) {
            p.intensity     = j.value("intensity",     p.intensity);
            p.edgeFade      = j.value("edgeFade",      p.edgeFade);
            p.depthFade     = j.value("depthFade",     p.depthFade);
            p.noiseTiling   = j.value("noiseTiling",   p.noiseTiling);
            p.noiseStrength = j.value("noiseStrength", p.noiseStrength);
            p.beamStrength  = j.value("beamStrength",  p.beamStrength);
            p.beamRadius    = j.value("beamRadius",    p.beamRadius);
            p.beamPower     = j.value("beamPower",     p.beamPower);
            p.beamGlow      = j.value("beamGlow",      p.beamGlow);
            p.isEnable      = j.value("isEnable",      p.isEnable);
            if (j.contains("halfExtents")) p.halfExtents = j["halfExtents"];
            if (j.contains("color"))       p.color       = j["color"];
        }

        void ToJson(json& j, const SmokeEffectParam& p) {
            j["color"]         = p.color;
            j["smokeColor"]    = p.smokeColor;
            j["riseSpeed"]     = p.riseSpeed;
            j["radius"]        = p.radius;
            j["noiseScale"]    = p.noiseScale;
            j["noiseStrength"] = p.noiseStrength;
            j["scrollSpeed"]   = p.scrollSpeed;
            j["fresnelPower"]  = p.fresnelPower;
            j["density"]       = p.density;
            j["noiseOctaves"]  = p.noiseOctaves;
            j["rimIntensity"]  = p.rimIntensity;
            j["isEnable"]      = p.isEnable;
            j["builtInBurstMotion"] = p.builtInBurstMotion;
        }
        void FromJson(const json& j, SmokeEffectParam& p) {
            if (j.contains("color"))      p.color      = j["color"];
            if (j.contains("smokeColor")) p.smokeColor = j["smokeColor"];
            p.riseSpeed     = j.value("riseSpeed",     p.riseSpeed);
            p.radius        = j.value("radius",        p.radius);
            p.noiseScale    = j.value("noiseScale",    p.noiseScale);
            p.noiseStrength = j.value("noiseStrength", p.noiseStrength);
            p.scrollSpeed   = j.value("scrollSpeed",   p.scrollSpeed);
            p.fresnelPower  = j.value("fresnelPower",  p.fresnelPower);
            p.density       = j.value("density",       p.density);
            p.noiseOctaves  = j.value("noiseOctaves",  p.noiseOctaves);
            p.rimIntensity  = j.value("rimIntensity",  p.rimIntensity);
            p.isEnable      = j.value("isEnable",      p.isEnable);
            p.builtInBurstMotion = j.value("builtInBurstMotion", p.builtInBurstMotion);
        }

        void ToJson(json& j, const LightningEffectParam& p) {
            j["color"]            = p.color;
            j["glowColor"]        = p.glowColor;
            j["branchColor"]      = p.branchColor;
            j["coreWidth"]        = p.coreWidth;
            j["solidness"]        = p.solidness;
            j["outlineIntensity"] = p.outlineIntensity;
            j["direction"]        = p.direction;
            j["bendAmount"]       = p.bendAmount;
            j["length"]           = p.length;
            j["width"]            = p.width;
            j["jitter"]           = p.jitter;
            j["segments"]         = p.segments;
            j["branches"]         = p.branches;
            j["branchJitter"]     = p.branchJitter;
            j["flickerRate"]      = p.flickerRate;
            j["glowPower"]        = p.glowPower;
            j["isEnable"]         = p.isEnable;
        }
        void FromJson(const json& j, LightningEffectParam& p) {
            if (j.contains("color"))       p.color       = j["color"];
            if (j.contains("glowColor"))   p.glowColor   = j["glowColor"];
            if (j.contains("branchColor")) p.branchColor = j["branchColor"];
            p.coreWidth        = j.value("coreWidth",        p.coreWidth);
            p.solidness        = j.value("solidness",        p.solidness);
            p.outlineIntensity = j.value("outlineIntensity", p.outlineIntensity);
            if (j.contains("direction")) p.direction = j["direction"];
            p.bendAmount   = j.value("bendAmount",   p.bendAmount);
            p.length       = j.value("length",       p.length);
            p.width        = j.value("width",        p.width);
            p.jitter       = j.value("jitter",       p.jitter);
            p.segments     = j.value("segments",     p.segments);
            p.branches     = j.value("branches",     p.branches);
            p.branchJitter = j.value("branchJitter", p.branchJitter);
            p.flickerRate  = j.value("flickerRate",  p.flickerRate);
            p.glowPower    = j.value("glowPower",    p.glowPower);
            p.isEnable     = j.value("isEnable",     p.isEnable);
        }

        void ToJson(json& j, const ShockwaveEffectParam& p) {
            j["color"]     = p.color;
            j["radius"]    = p.radius;
            j["duration"]  = p.duration;
            j["thickness"] = p.thickness;
            j["isEnable"]  = p.isEnable;
        }
        void FromJson(const json& j, ShockwaveEffectParam& p) {
            if (j.contains("color")) p.color = j["color"];
            p.radius    = j.value("radius",    p.radius);
            p.duration  = j.value("duration",  p.duration);
            p.thickness = j.value("thickness", p.thickness);
            p.isEnable  = j.value("isEnable",  p.isEnable);
        }

        json MotionsToJson(const std::vector<VfxMotion>& motions) {
            json arr = json::array();
            for (const auto& m : motions) {
                json mj;
                mj["type"]         = static_cast<int>(m.type);
                mj["target"]       = static_cast<int>(m.target);
                mj["ease"]         = static_cast<int>(m.ease);
                mj["startTime"]    = m.startTime;
                mj["window"]       = m.window;
                mj["duration"]     = m.duration;
                mj["velocity"]     = m.velocity;
                mj["acceleration"] = m.acceleration;
                mj["amplitude"]    = m.amplitude;
                mj["frequency"]    = m.frequency;
                mj["scaleStart"]   = m.scaleStart;
                mj["scaleEnd"]     = m.scaleEnd;
                mj["colorStart"]   = m.colorStart;
                mj["colorEnd"]     = m.colorEnd;
                mj["fadeIn"]       = m.fadeIn;
                mj["fadeOut"]      = m.fadeOut;
                arr.push_back(mj);
            }
            return arr;
        }
        void MotionsFromJson(const json& arr, std::vector<VfxMotion>& motions) {
            motions.clear();
            if (!arr.is_array()) return;
            for (const auto& mj : arr) {
                VfxMotion m;
                m.type      = static_cast<VfxMotionType>(mj.value("type", static_cast<int>(m.type)));
                m.target    = static_cast<VfxMotionTarget>(mj.value("target", static_cast<int>(m.target)));
                m.ease      = static_cast<VfxEase>(mj.value("ease", static_cast<int>(m.ease)));
                m.startTime = mj.value("startTime", m.startTime);
                m.window    = mj.value("window",    m.window);
                m.duration  = mj.value("duration",  m.duration);
                if (mj.contains("velocity"))     m.velocity     = mj["velocity"];
                if (mj.contains("acceleration")) m.acceleration = mj["acceleration"];
                m.amplitude  = mj.value("amplitude",  m.amplitude);
                m.frequency  = mj.value("frequency",  m.frequency);
                m.scaleStart = mj.value("scaleStart", m.scaleStart);
                m.scaleEnd   = mj.value("scaleEnd",   m.scaleEnd);
                if (mj.contains("colorStart")) m.colorStart = mj["colorStart"];
                if (mj.contains("colorEnd"))   m.colorEnd   = mj["colorEnd"];
                m.fadeIn  = mj.value("fadeIn",  m.fadeIn);
                m.fadeOut = mj.value("fadeOut", m.fadeOut);
                motions.push_back(m);
            }
        }

        // サブ効果1個分: type に対応するパラメータブロックだけ書く
        json SubEffectToJson(const VfxSubEffect& sub) {
            json j;
            j["type"]    = static_cast<int>(sub.type);
            j["label"]   = sub.label;
            j["enabled"] = sub.enabled;
            j["offset"]  = sub.offset;
            switch (sub.type) {
            case VfxSubEffectType::LightVolume: ToJson(j["lightVolume"], sub.lightVolume); break;
            case VfxSubEffectType::Smoke:       ToJson(j["smoke"],       sub.smoke);       break;
            case VfxSubEffectType::Lightning:   ToJson(j["lightning"],   sub.lightning);   break;
            case VfxSubEffectType::Shockwave:   ToJson(j["shockwave"],   sub.shockwave);   break;
            }
            if (!sub.motions.empty()) j["motions"] = MotionsToJson(sub.motions);
            return j;
        }
        void SubEffectFromJson(const json& j, VfxSubEffect& sub) {
            sub.type    = static_cast<VfxSubEffectType>(j.value("type", static_cast<int>(sub.type)));
            sub.label   = j.value("label",   sub.label);
            sub.enabled = j.value("enabled", sub.enabled);
            if (j.contains("offset")) sub.offset = j["offset"];
            if (j.contains("lightVolume")) FromJson(j["lightVolume"], sub.lightVolume);
            if (j.contains("smoke"))       FromJson(j["smoke"],       sub.smoke);
            if (j.contains("lightning"))   FromJson(j["lightning"],   sub.lightning);
            if (j.contains("shockwave"))   FromJson(j["shockwave"],   sub.shockwave);
            if (j.contains("motions"))     MotionsFromJson(j["motions"], sub.motions);
        }

    } // namespace

    // -------------------------------------------------------
    void VfxEffectAsset::SaveToJson(const std::string& filePath) const {
        json j;
        j["name"]     = name;
        j["useTrail"] = useTrail;

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
        t["emissiveIntensity"] = trail.emissiveIntensity;
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

        // ディゾルブ (溶けて消える)
        t["dissolveStrength"]  = trail.dissolveStrength;
        t["dissolveEdgeWidth"] = trail.dissolveEdgeWidth;
        t["dissolveEdgeColor"] = trail.dissolveEdgeColor;

        // 3D プリミティブ
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

        // --- サブ効果リスト（同じ種類を複数持てる） ---
        j["subEffects"] = json::array();
        for (const auto& sub : subEffects) {
            j["subEffects"].push_back(SubEffectToJson(sub));
        }

        // --- Motions (エフェクト全体の動き) ---
        j["motions"] = MotionsToJson(motions);

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

        name     = j.value("name",     name);
        useTrail = j.value("useTrail", useTrail);

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
            trail.emissiveIntensity = t.value("emissiveIntensity", trail.emissiveIntensity);
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

            // ディゾルブ (溶けて消える)
            trail.dissolveStrength  = t.value("dissolveStrength",  trail.dissolveStrength);
            trail.dissolveEdgeWidth = t.value("dissolveEdgeWidth", trail.dissolveEdgeWidth);
            if (t.contains("dissolveEdgeColor")) trail.dissolveEdgeColor = t["dissolveEdgeColor"];

            // 3D プリミティブ (旧 JSON 互換: primitive キーがなければデフォルト)
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

        // --- サブ効果 ---
        subEffects.clear();
        if (j.contains("subEffects") && j["subEffects"].is_array()) {
            // 新形式: サブ効果リスト
            for (const auto& sj : j["subEffects"]) {
                VfxSubEffect sub;
                SubEffectFromJson(sj, sub);
                subEffects.push_back(std::move(sub));
            }
        } else {
            // 旧形式: 各タイプ1個ずつ + useXXX フラグ → 使用中のものだけリスト化
            const bool useLightVolume = j.value("useLightVolume", true);
            const bool useSmoke       = j.value("useSmoke",       false);
            const bool useLightning   = j.value("useLightning",   false);
            const bool useShockwave   = j.value("useShockwave",   false);

            auto migrate = [&](bool used, const char* key, VfxSubEffectType type) {
                if (!used) return;
                VfxSubEffect sub;
                sub.type = type;
                if (j.contains(key)) {
                    // 有効/無効はインスタンスの enabled に一本化する
                    // （メッシュ側が参照する param.isEnable は常に true へ正規化）
                    switch (type) {
                    case VfxSubEffectType::LightVolume: FromJson(j[key], sub.lightVolume); sub.enabled = sub.lightVolume.isEnable; sub.lightVolume.isEnable = true; break;
                    case VfxSubEffectType::Smoke:       FromJson(j[key], sub.smoke);       sub.enabled = sub.smoke.isEnable;       sub.smoke.isEnable = true;       break;
                    case VfxSubEffectType::Lightning:   FromJson(j[key], sub.lightning);   sub.enabled = sub.lightning.isEnable;   sub.lightning.isEnable = true;   break;
                    case VfxSubEffectType::Shockwave:   FromJson(j[key], sub.shockwave);   sub.enabled = sub.shockwave.isEnable;   sub.shockwave.isEnable = true;   break;
                    }
                }
                subEffects.push_back(std::move(sub));
            };
            migrate(useLightVolume, "lightVolume", VfxSubEffectType::LightVolume);
            migrate(useSmoke,       "smoke",       VfxSubEffectType::Smoke);
            migrate(useLightning,   "lightning",   VfxSubEffectType::Lightning);
            migrate(useShockwave,   "shockwave",   VfxSubEffectType::Shockwave);
        }

        // --- Motions (旧 JSON 互換: motions キーが無ければ空=従来挙動) ---
        motions.clear();
        if (j.contains("motions")) MotionsFromJson(j["motions"], motions);

        return true;
    }

} // namespace YoRigine
