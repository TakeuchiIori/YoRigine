#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include <utility>

namespace YoRigine {

    VfxEffectAsset VfxMeshEditor::MakePreset(VfxPreset preset)
    {
        VfxEffectAsset a;

        auto addVolume = [&a](const Vector3& halfExtents, const Vector4& color, float intensity, bool waypointBeam = false) {
            VfxElement sub;
            sub.type = VfxElementType::LightVolume;
            sub.lightVolume.halfExtents = halfExtents;
            sub.lightVolume.color = color;
            sub.lightVolume.intensity = intensity;
            if (waypointBeam) ApplyDefaultElementMotions(sub);
            a.elements.push_back(std::move(sub));
        };

        switch (preset) {
        case VfxPreset::TrailOnly:
            a.useTrail = true;
            break;

        case VfxPreset::VolumeOnly:
            a.useTrail = false;
            addVolume({ 2.f, 1.5f, 5.f }, { 1.f, 0.9f, 0.f, 0.15f }, 1.0f);
            break;

        case VfxPreset::WaypointBeam:
            a.useTrail = false;
            a.name = "WayPoint";
            addVolume({ 7.1f, 16.9f, 14.1f }, { 1.f, 0.9019608f, 0.f, 1.0f }, 0.73f, true);
            break;

        case VfxPreset::Sword:
            a.useTrail = true;
            a.trail.widthStart = 0.05f;
            a.trail.widthEnd = 0.f;
            a.trail.lifetime = 0.25f;
            a.trail.maxPoints = 48;
            a.trail.colorStart = { 1.f, 0.95f, 0.7f, 1.f };
            a.trail.colorEnd = { 0.8f, 0.5f, 0.1f, 0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.3f;
            addVolume({ 0.04f, 0.04f, 0.6f }, { 1.f, 0.9f, 0.5f, 0.18f }, 1.5f);
            break;

        case VfxPreset::Magic:
            a.useTrail = true;
            a.trail.widthStart = 0.4f;
            a.trail.widthEnd = 0.05f;
            a.trail.lifetime = 1.2f;
            a.trail.maxPoints = 96;
            a.trail.colorStart = { 0.4f, 0.2f, 1.f, 1.f };
            a.trail.colorEnd = { 0.2f, 0.6f, 1.f, 0.f };
            a.trail.blendMode = BlendMode::kBlendModeAdd;
            a.trail.uvScrollSpeed = 0.8f;
            addVolume({ 1.5f, 1.5f, 4.f }, { 0.5f, 0.3f, 1.f, 0.25f }, 3.f);
            break;

        case VfxPreset::Explosion: {
            a.useTrail = false;

            VfxMotion life;
            life.type = VfxMotionType::BurstGrow;
            life.duration = 2.2f;
            a.motions.push_back(life);

            VfxElement fire;
            fire.type = VfxElementType::NoiseVolume;
            fire.label = "火球";
            fire.smoke.color = { 4.0f, 1.8f, 0.6f, 1.0f };
            fire.smoke.radius = 1.5f;
            fire.smoke.riseSpeed = 0.f;
            fire.smoke.noiseStrength = 0.9f;
            fire.smoke.rimIntensity = 3.0f;
            fire.smoke.builtInBurstMotion = false;

            VfxMotion pop;
            pop.type = VfxMotionType::ScaleOverLife;
            pop.ease = VfxEase::EaseOutExpo;
            pop.window = 0.5f;
            pop.scaleStart = 0.15f;
            pop.scaleEnd = 1.5f;
            fire.motions.push_back(pop);

            VfxMotion color;
            color.type = VfxMotionType::ColorOverLife;
            color.ease = VfxEase::EaseInQuad;
            color.window = 2.2f;
            color.colorStart = { 1.0f, 1.0f, 1.0f, 1.0f };
            color.colorEnd = { 0.05f, 0.05f, 0.06f, 0.85f };
            fire.motions.push_back(color);

            VfxMotion rise;
            rise.type = VfxMotionType::Rise;
            rise.startTime = 0.4f;
            rise.window = 1.8f;
            rise.velocity = { 0.f, 1.0f, 0.f };
            rise.amplitude = 1.0f;
            fire.motions.push_back(rise);

            VfxMotion fade;
            fade.type = VfxMotionType::FadeInOut;
            fade.window = 2.2f;
            fade.fadeIn = 0.03f;
            fade.fadeOut = 0.7f;
            fire.motions.push_back(fade);
            a.elements.push_back(std::move(fire));

            VfxElement ring;
            ring.type = VfxElementType::ShockwaveRing;
            ring.label = "衝撃波";
            ring.shockwave.color = { 6.0f, 2.5f, 1.0f, 1.0f };
            ring.shockwave.radius = 3.5f;
            ring.shockwave.duration = 0.5f;
            ring.shockwave.thickness = 0.18f;

            VfxMotion showRing;
            showRing.type = VfxMotionType::Visibility;
            showRing.window = 0.5f;
            ring.motions.push_back(showRing);

            VfxMotion fadeRing;
            fadeRing.type = VfxMotionType::FadeInOut;
            fadeRing.window = 0.5f;
            fadeRing.fadeIn = 0.f;
            fadeRing.fadeOut = 0.25f;
            ring.motions.push_back(fadeRing);
            a.elements.push_back(std::move(ring));

            VfxElement flash;
            flash.type = VfxElementType::LightningBolt;
            flash.label = "閃光";
            flash.lightning.color = { 8.0f, 8.0f, 10.0f, 1.0f };
            flash.lightning.glowColor = { 6.0f, 6.0f, 9.0f, 1.0f };
            flash.lightning.branchColor = { 6.0f, 6.0f, 9.0f, 1.0f };
            flash.lightning.length = 3.0f;
            flash.lightning.width = 0.35f;
            flash.lightning.branches = 6;
            flash.lightning.flickerRate = 40.0f;

            VfxMotion showFlash;
            showFlash.type = VfxMotionType::Visibility;
            showFlash.window = 0.18f;
            flash.motions.push_back(showFlash);

            VfxMotion flicker;
            flicker.type = VfxMotionType::Flicker;
            flicker.amplitude = 0.5f;
            flicker.frequency = 60.0f;
            flash.motions.push_back(flicker);
            a.elements.push_back(std::move(flash));
            break;
        }

        default:
            break;
        }

        return a;
    }

} // namespace YoRigine
#endif
