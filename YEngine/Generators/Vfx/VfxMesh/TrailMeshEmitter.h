#pragma once
// ===========================================================
// TrailMeshEmitter.h
// ===========================================================
#include <memory>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "VfxEffectAsset.h"
#include "TrailMesh.h"
#include "Systems/Camera/Camera.h"

namespace YoRigine {

    // ★ HLSL MeshTrailParams と 1:1 で一致させること
    // レイアウト (16バイト境界):
    //   Row0 : colorInner[4]
    //   Row1 : colorOuter[4]
    //   Row2 : rimColor[4]
    //   Row3 : softness, glowPower, distortion, time
    //   Row4 : energyIntensity, energySpeed, sparkleAmount, sparkleSpeed
    //   Row5 : fresnelStrength, trailSharpness, colorWaveFreq, colorWaveAmp
    //   Row6 : uvScrollSpeed, noiseOctaves, _pad0, _pad1
    // 合計 112 bytes < 256 (1 CBV スロット内に収まる)
    struct MeshTrailParamsCB
    {
        float colorInner[4];      // Row0
        float colorOuter[4];      // Row1
        float rimColor[4];        // Row2 ★NEW

        float softness;           // Row3
        float glowPower;
        float distortion;
        float time;

        float energyIntensity;    // Row4 ★NEW
        float energySpeed;
        float sparkleAmount;
        float sparkleSpeed;

        float fresnelStrength;    // Row5 ★NEW
        float trailSharpness;
        float colorWaveFreq;
        float colorWaveAmp;

        float uvScrollSpeed;      // Row6 ★NEW
        float noiseOctaves;
        float _pad0;
        float _pad1;
    };

    class TrailMeshEmitter
    {
    public:
        TrailMeshEmitter() = default;
        ~TrailMeshEmitter();

        bool LoadAsset(const std::string& filePath);
        void SetAsset(const YoRigine::VfxEffectAsset& asset);

        void AddPoint(const Vector3& tip, const Vector3& root);
        void AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir);

        void Update(float deltaTime);
        void Draw();
        void Draw(ID3D12GraphicsCommandList* cmdList);

        void Play();
        void Stop();

    public:
        YoRigine::TrailMesh*              GetTrailMesh() const { return trailMesh_.get(); }
        const YoRigine::VfxEffectAsset&   GetAsset()     const { return asset_; }
        void SetCamera(Camera* camera) { camera_ = camera; }

    private:
        void EnsureCB();
        void UpdateCB();

    private:
        YoRigine::VfxEffectAsset             asset_;
        std::unique_ptr<YoRigine::TrailMesh> trailMesh_;
        bool isPlaying_ = false;

        Camera* camera_ = nullptr;
        float   time_   = 0.0f;

        Microsoft::WRL::ComPtr<ID3D12Resource> trailCBResource_;
        MeshTrailParamsCB* trailCBMapped_ = nullptr;
    };

} // namespace YoRigine
