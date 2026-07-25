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
#include "VfxParamsJson.h" // geom/mat variant の to_json/from_json
using json = nlohmann::json;

namespace YoRigine {

    // ===========================================================
    // 旧モノリシックエレメント → 二軸合成（Composed）への変換
    //   NoiseVolume  → Sphere × Noise
    //   ShockwaveRing→ Plane  × Shockwave（旧はカメラ向きクワッド）
    //   LightVolume / LightningBolt → Monolithic のまま（専用メッシュ）
    // ===========================================================
    static void MigrateElementToComposed(VfxElement& sub) {
        switch (sub.type) {
        case VfxElementType::NoiseVolume: {
            sub.kind = VfxElementKind::Composed;
            SphereGeomParams g;
            g.radius = sub.smoke.radius;
            sub.geom = g;
            NoiseMatParams m;
            m.color              = sub.smoke.color;
            m.smokeColor         = sub.smoke.smokeColor;
            m.radius             = sub.smoke.radius;
            m.noiseScale         = sub.smoke.noiseScale;
            m.noiseStrength      = sub.smoke.noiseStrength;
            m.scrollSpeed        = sub.smoke.scrollSpeed;
            m.fresnelPower       = sub.smoke.fresnelPower;
            m.density            = sub.smoke.density;
            m.noiseOctaves       = sub.smoke.noiseOctaves;
            m.rimIntensity       = sub.smoke.rimIntensity;
            m.builtInBurstMotion = sub.smoke.builtInBurstMotion;
            sub.mat = m;
            break;
        }
        case VfxElementType::ShockwaveRing: {
            sub.kind = VfxElementKind::Composed;
            PlaneGeomParams g;                  // 旧はカメラ向きクワッド
            g.size      = sub.shockwave.radius; // 最大半径 = クワッド半辺
            g.billboard = true;
            sub.geom = g;
            ShockwaveMatParams m;
            m.color     = sub.shockwave.color;
            m.thickness = sub.shockwave.thickness;
            sub.mat = m;
            // 旧 duration は膨張アニメ用だったが、膨張は Module(ScaleOverLife) に移行。
            // 旧アセットにも自動で膨張モジュールを1本足しておく。
            if (sub.modules.empty()) {
                VfxModule grow;
                grow.type       = VfxModuleType::ScaleOverLife;
                grow.ease       = VfxEase::EaseOutExpo;
                grow.window     = std::max(sub.shockwave.duration, 0.05f);
                grow.scaleStart = 0.05f;
                grow.scaleEnd   = 1.0f;
                sub.modules.push_back(grow);
            }
            break;
        }
        case VfxElementType::LightVolume:
        case VfxElementType::LightningBolt:
        default:
            sub.kind = VfxElementKind::Monolithic; // 専用メッシュはそのまま
            break;
        }
    }

    // -------------------------------------------------------
    const char* VfxElementTypeName(VfxElementType type) {
        switch (type) {
        case VfxElementType::LightVolume:   return "LightVolume";
        case VfxElementType::NoiseVolume:   return "NoiseVolume";
        case VfxElementType::LightningBolt: return "LightningBolt";
        case VfxElementType::ShockwaveRing: return "ShockwaveRing";
        }
        return "Unknown";
    }

    VfxModuleTarget ModuleTargetFor(VfxElementType type) {
        switch (type) {
        case VfxElementType::LightVolume:   return VfxModuleTarget::LightVolume;
        case VfxElementType::NoiseVolume:   return VfxModuleTarget::NoiseVolume;
        case VfxElementType::LightningBolt: return VfxModuleTarget::LightningBolt;
        case VfxElementType::ShockwaveRing: return VfxModuleTarget::ShockwaveRing;
        }
        return VfxModuleTarget::All;
    }

    // -------------------------------------------------------
    // ワンショット寿命: Lifetime モジュール（全体/エレメント個別）優先、
    // 無ければ従来ヒューリスティック
    float VfxEffectAsset::OneShotDuration() const {
        float burst = 0.f;
        auto scan = [&burst](const std::vector<VfxModule>& ms) {
            for (const auto& m : ms) {
                if (m.type == VfxModuleType::Lifetime) burst = std::max(burst, m.duration);
            }
        };
        scan(modules);
        for (const auto& sub : elements) scan(sub.modules);
        if (burst > 0.f) return std::max(burst, 0.01f);

        // ヒューリスティック: Noise系があれば最低2秒、Shockwave はその膨張時間
        bool  hasNoiseVolume = false;
        float swDur    = 0.f;
        for (const auto& sub : elements) {
            if (!sub.enabled) continue;
            if (sub.kind == VfxElementKind::Composed) {
                // 二軸合成: マテリアル種別で判定
                if (sub.MaterialType() == VfxMaterialType::Noise) hasNoiseVolume = true;
                // Composed の Shockwave は膨張を Module(ScaleOverLife) が持つので、
                // 寿命は Lifetime / モジュール窓 / 下の Noise ヒューリスティックに委ねる。
            } else {
                if (sub.type == VfxElementType::NoiseVolume)   hasNoiseVolume = true;
                if (sub.type == VfxElementType::ShockwaveRing) swDur = std::max(swDur, sub.shockwave.duration);
            }
        }
        return std::max(hasNoiseVolume
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
    // モジュール評価: target（または All）に一致する動きを共有状態へ加算する。
    // 位置移動は position だけでなく端点(boltStart/End)にも同じオフセットを掛け、
    // LightningBolt（端点駆動）も一緒に動くようにする。
    //
    // 全モジュール共通のタイミング:
    //   startTime … この時刻まで不発（Visibility だけは「表示ウィンドウ」として解釈）
    //   window    … 効果時間。移動系はこの時間で停止、補間系はこの時間で 0→1。
    //               0 の場合は無限（補間系はワンショット寿命 lifetime/progress を使う）
    // （寿命=progress は Spawner が OneShotDuration() で算出済み。Lifetime はここでは何もしない）
    void EvaluateModuleList(const std::vector<VfxModule>& modules,
                            VfxModuleTarget target, VfxEvalState& s) {
        for (const auto& m : modules) {
            if (m.target != VfxModuleTarget::All && m.target != target) continue;

            // ---- Visibility: 表示ウィンドウ外なら非表示にするだけ ----
            if (m.type == VfxModuleType::Visibility) {
                const bool inWindow = (s.age >= m.startTime) &&
                    (m.window <= 0.f || s.age <= m.startTime + m.window);
                if (!inWindow) s.visible = false;
                continue;
            }

            if (m.type == VfxModuleType::Lifetime) continue; // 寿命定義のみ（ここでは何もしない）

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
            case VfxModuleType::Move:
                shift(m.velocity.x * te, m.velocity.y * te, m.velocity.z * te);
                break;

            case VfxModuleType::Rise:
                shift(0.f, m.velocity.y * m.amplitude * te, 0.f);
                break;

            case VfxModuleType::ScalePulse:
                s.scale *= 1.0f + m.amplitude * std::sin(s.age * m.frequency * 6.2831853f);
                break;

            case VfxModuleType::ScaleOverLife:
                s.scale *= m.scaleStart + (m.scaleEnd - m.scaleStart) * eased;
                break;

            case VfxModuleType::ColorOverLife:
                s.colorTint.x *= m.colorStart.x + (m.colorEnd.x - m.colorStart.x) * eased;
                s.colorTint.y *= m.colorStart.y + (m.colorEnd.y - m.colorStart.y) * eased;
                s.colorTint.z *= m.colorStart.z + (m.colorEnd.z - m.colorStart.z) * eased;
                s.colorTint.w *= m.colorStart.w + (m.colorEnd.w - m.colorStart.w) * eased;
                break;

            case VfxModuleType::FadeInOut: {
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

            case VfxModuleType::Accelerate:
                // x(t) = v0*t + 0.5*a*t^2 （重力・打ち上げ・吹き飛ばし）
                shift(m.velocity.x * te + 0.5f * m.acceleration.x * te * te,
                      m.velocity.y * te + 0.5f * m.acceleration.y * te * te,
                      m.velocity.z * te + 0.5f * m.acceleration.z * te * te);
                break;

            case VfxModuleType::Orbit: {
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

            case VfxModuleType::Shake: {
                // 位相をずらした sin の合成で疑似ランダムに揺らす（決定的）
                const float f = m.frequency * 6.2831853f;
                shift(std::sin(s.age * f + 12.9f) * std::sin(s.age * f * 1.31f + 4.7f) * m.amplitude,
                      std::sin(s.age * f * 0.87f + 78.2f) * std::sin(s.age * f * 1.17f + 1.3f) * m.amplitude,
                      std::sin(s.age * f * 1.13f + 37.7f) * std::sin(s.age * f * 0.73f + 9.1f) * m.amplitude);
                break;
            }

            case VfxModuleType::Flicker: {
                // frequency 回/秒で段階的に変わるランダム明滅
                const float n = StepNoise(s.age * std::max(m.frequency, 0.01f));
                s.colorTint.w *= std::max(0.0f, 1.0f - m.amplitude * n);
                break;
            }

            case VfxModuleType::BeamPulse: {
                const float wave = 0.5f + 0.5f * std::sin(s.age * m.frequency * 6.2831853f);
                const float radius = 1.0f + m.amplitude * (wave * 2.0f - 1.0f);
                s.beamRadiusScale *= std::max(0.01f, radius);
                s.beamGlowScale *= std::max(0.0f, 1.0f + m.amplitude * wave);
                break;
            }

            default:
                break;
            }
        }
    }

    void EvaluateModules(const VfxEffectAsset& a, VfxModuleTarget target, VfxEvalState& s) {
        EvaluateModuleList(a.modules, target, s);
    }

    void EvaluateElementModules(const VfxEffectAsset& a,
                                  const VfxElement& sub, VfxEvalState& s) {
        // 二軸合成エレメントは種別による絞り込みがないので All 扱い
        // （自身の modules は target=All 既定なので常に適用される）
        const VfxModuleTarget target = (sub.kind == VfxElementKind::Composed)
            ? VfxModuleTarget::All : ModuleTargetFor(sub.type);
        EvaluateModuleList(a.modules, target, s);   // 全体モジュール（対象一致分）
        EvaluateModuleList(sub.modules, target, s); // このエレメント専用のモジュール
    }

    // ===========================================================
    // JSON ヘルパ（エレメントパラメータの読み書き）
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

        json ModulesToJson(const std::vector<VfxModule>& modules) {
            json arr = json::array();
            for (const auto& m : modules) {
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
        void ModulesFromJson(const json& arr, std::vector<VfxModule>& modules) {
            modules.clear();
            if (!arr.is_array()) return;
            for (const auto& mj : arr) {
                VfxModule m;
                m.type      = static_cast<VfxModuleType>(mj.value("type", static_cast<int>(m.type)));
                m.target    = static_cast<VfxModuleTarget>(mj.value("target", static_cast<int>(m.target)));
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
                modules.push_back(m);
            }
        }

        // エレメント1個分の書き出し。
        //   Composed  … kind + geom/mat variant を保存
        //   Monolithic… kind + type + 専用パラメータを保存
        json ElementToJson(const VfxElement& sub) {
            json j;
            j["kind"]      = static_cast<int>(sub.kind);
            j["label"]     = sub.label;
            j["enabled"]   = sub.enabled;
            j["offset"]    = sub.offset;
            j["rotation"]  = sub.rotation;
            j["blendMode"] = sub.blendModeOverride; // -1=マテリアル既定

            if (sub.kind == VfxElementKind::Composed) {
                j["geom"] = sub.geom; // variant to_json（_index + _data）
                j["mat"]  = sub.mat;
            } else {
                j["type"]     = static_cast<int>(sub.type);
                j["typeName"] = VfxElementTypeName(sub.type);
                switch (sub.type) {
                case VfxElementType::LightVolume:   ToJson(j["lightVolume"], sub.lightVolume); break;
                case VfxElementType::LightningBolt: ToJson(j["lightning"],   sub.lightning);   break;
                default: break;
                }
            }
            if (!sub.modules.empty()) j["modules"] = ModulesToJson(sub.modules);
            return j;
        }
        VfxElementType ParseElementType(const json& j, VfxElementType fallback) {
            if (j.contains("typeName") && j["typeName"].is_string()) {
                const std::string s = j["typeName"].get<std::string>();
                if (s == "LightVolume") return VfxElementType::LightVolume;
                if (s == "NoiseVolume" || s == "VolumeSmoke" || s == "Smoke") return VfxElementType::NoiseVolume;
                if (s == "LightningBolt" || s == "Lightning") return VfxElementType::LightningBolt;
                if (s == "ShockwaveRing" || s == "Shockwave") return VfxElementType::ShockwaveRing;
            }
            return static_cast<VfxElementType>(j.value("type", static_cast<int>(fallback)));
        }
        void ElementFromJson(const json& j, VfxElement& sub) {
            sub.label   = j.value("label",   sub.label);
            sub.enabled = j.value("enabled", sub.enabled);
            sub.blendModeOverride = j.value("blendMode", sub.blendModeOverride); // 無ければ-1(既定)
            if (j.contains("offset"))   sub.offset   = j["offset"];
            if (j.contains("rotation")) sub.rotation = j["rotation"];
            if (j.contains("modules"))       ModulesFromJson(j["modules"], sub.modules);
            else if (j.contains("motions"))  ModulesFromJson(j["motions"], sub.modules); // 旧キー互換

            // 新形式: kind があればそれに従う（移行不要）
            if (j.contains("kind")) {
                sub.kind = static_cast<VfxElementKind>(j.value("kind", 0));
                if (sub.kind == VfxElementKind::Composed) {
                    if (j.contains("geom")) sub.geom = j["geom"].get<VfxGeometryParams>();
                    if (j.contains("mat"))  sub.mat  = j["mat"].get<VfxMaterialParams>();
                } else {
                    sub.type = ParseElementType(j, sub.type);
                    if (j.contains("lightVolume")) FromJson(j["lightVolume"], sub.lightVolume);
                    if (j.contains("lightning"))   FromJson(j["lightning"],   sub.lightning);
                }
                return;
            }

            // 旧形式: type ベース → 読み込んでから Composed へ移行
            sub.type = ParseElementType(j, sub.type);
            if (j.contains("lightVolume")) FromJson(j["lightVolume"], sub.lightVolume);
            if (j.contains("smoke"))       FromJson(j["smoke"],       sub.smoke);
            if (j.contains("lightning"))   FromJson(j["lightning"],   sub.lightning);
            if (j.contains("shockwave"))   FromJson(j["shockwave"],   sub.shockwave);
            MigrateElementToComposed(sub);
        }

    } // namespace

    // -------------------------------------------------------
    void VfxEffectAsset::SaveToJson(const std::string& filePath) const {
        json j;
        j["name"]        = name;
        j["useTrail"]    = useTrail;
        j["loopForever"] = loopForever;

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

        // --- エレメントリスト（同じ種類を複数持てる） ---
        j["elements"] = json::array();
        for (const auto& sub : elements) {
            j["elements"].push_back(ElementToJson(sub));
        }

        // --- Modules (エフェクト全体の動き) ---
        j["modules"] = ModulesToJson(modules);

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

        name        = j.value("name",        name);
        useTrail    = j.value("useTrail",    useTrail);
        loopForever = j.value("loopForever", loopForever);

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

        // --- エレメント ---
        elements.clear();
        const json* elementArray = nullptr;
        if (j.contains("elements") && j["elements"].is_array()) {
            elementArray = &j["elements"];
        } else if (j.contains("subEffects") && j["subEffects"].is_array()) {
            elementArray = &j["subEffects"];
        }

        if (elementArray) {
            // 新形式 elements / 旧形式 subEffects の両方を読む
            for (const auto& sj : *elementArray) {
                VfxElement sub;
                ElementFromJson(sj, sub);
                elements.push_back(std::move(sub));
            }
        } else {
            // 旧形式: 各タイプ1個ずつ + useXXX フラグ → 使用中のものだけリスト化
            const bool useLightVolume = j.value("useLightVolume", true);
            const bool useSmoke       = j.value("useSmoke",       false);
            const bool useLightning   = j.value("useLightning",   false);
            const bool useShockwave   = j.value("useShockwave",   false);

            auto migrate = [&](bool used, const char* key, VfxElementType type) {
                if (!used) return;
                VfxElement sub;
                sub.type = type;
                if (j.contains(key)) {
                    // 有効/無効はインスタンスの enabled に一本化する
                    // （メッシュ側が参照する param.isEnable は常に true へ正規化）
                    switch (type) {
                    case VfxElementType::LightVolume: FromJson(j[key], sub.lightVolume); sub.enabled = sub.lightVolume.isEnable; sub.lightVolume.isEnable = true; break;
                    case VfxElementType::NoiseVolume:       FromJson(j[key], sub.smoke);       sub.enabled = sub.smoke.isEnable;       sub.smoke.isEnable = true;       break;
                    case VfxElementType::LightningBolt:   FromJson(j[key], sub.lightning);   sub.enabled = sub.lightning.isEnable;   sub.lightning.isEnable = true;   break;
                    case VfxElementType::ShockwaveRing:   FromJson(j[key], sub.shockwave);   sub.enabled = sub.shockwave.isEnable;   sub.shockwave.isEnable = true;   break;
                    }
                }
                MigrateElementToComposed(sub); // 旧 → Composed へ変換
                elements.push_back(std::move(sub));
            };
            migrate(useLightVolume, "lightVolume", VfxElementType::LightVolume);
            migrate(useSmoke,       "smoke",       VfxElementType::NoiseVolume);
            migrate(useLightning,   "lightning",   VfxElementType::LightningBolt);
            migrate(useShockwave,   "shockwave",   VfxElementType::ShockwaveRing);
        }

        // --- Modules (旧 JSON 互換: modules/motions どちらも無ければ空=従来挙動) ---
        modules.clear();
        if (j.contains("modules"))      ModulesFromJson(j["modules"], modules);
        else if (j.contains("motions")) ModulesFromJson(j["motions"], modules); // 旧キー互換

        return true;
    }

} // namespace YoRigine
