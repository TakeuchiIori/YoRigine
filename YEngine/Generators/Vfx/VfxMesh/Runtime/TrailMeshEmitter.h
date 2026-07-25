#pragma once
// ===========================================================
// TrailMeshEmitter.h
//
// TrailMesh の PSO バインド・CB 管理・再生制御を担うラッパー。
// 所有者（PlayerSword など）はこのクラスを直接保持し、
// 毎フレーム AddPoint() → Update() → Draw() を呼ぶ。
//
// TrailMesh との役割分担:
//   TrailMesh     - 頂点バッファ（形状・寿命管理）
//   TrailMeshEmitter - PSO 選択・CB 生成・再生フラグ管理
// ===========================================================
#include <memory>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include <Vfx/VfxMesh/Core/VfxEffectAsset.h>
#include <Vfx/VfxMesh/Effects/TrailMesh.h>
#include "Systems/Camera/Camera.h"

namespace YoRigine {

    // ===========================================================
    // MeshTrailParamsCB
    //
    // HLSL の MeshTrailParams と 1:1 で一致させること。
    // レイアウト（16 バイト境界）:
    //   Row0 : colorInner[4]
    //   Row1 : colorOuter[4]
    //   Row2 : rimColor[4]
    //   Row3 : softness, glowPower, distortion, time
    //   Row4 : energyIntensity, energySpeed, sparkleAmount, sparkleSpeed
    //   Row5 : fresnelStrength, trailSharpness, colorWaveFreq, colorWaveAmp
    //   Row6 : uvScrollSpeed, noiseOctaves, dissolveStrength, dissolveEdgeWidth
    //   Row7 : dissolveEdgeColor[4]
    //   Row8 : emissiveIntensity, _pad2, _pad3, _pad4
    // 合計 144 bytes（256 バイト以内に収まる）
    // ===========================================================
    struct MeshTrailParamsCB
    {
        float colorInner[4];      // Row0
        float colorOuter[4];      // Row1
        float rimColor[4];        // Row2

        float softness;           // Row3
        float glowPower;
        float distortion;
        float time;

        float energyIntensity;    // Row4
        float energySpeed;
        float sparkleAmount;
        float sparkleSpeed;

        float fresnelStrength;    // Row5
        float trailSharpness;
        float colorWaveFreq;
        float colorWaveAmp;

        float uvScrollSpeed;      // Row6
        float noiseOctaves;
        float dissolveStrength;   ///< 溶けて消える強度 (0=OFF)
        float dissolveEdgeWidth;  ///< 侵食エッジの帯幅

        float dissolveEdgeColor[4]; // Row7 侵食エッジの HDR 発光色

        float emissiveIntensity;  // Row8 発光強度 (0=消灯)
        float _pad2;
        float _pad3;
        float _pad4;
    };

    class TrailMeshEmitter
    {
    public:
        TrailMeshEmitter() = default;
        ~TrailMeshEmitter();

        // ── アセット設定 ──────────────────────────────────────

        /// JSON ファイルからアセットをロードして TrailMesh を初期化する
        bool LoadAsset(const std::string& filePath);

        /// アセットを直接セットして TrailMesh を初期化する（エディタ用）
        void SetAsset(const YoRigine::VfxEffectAsset& asset);

        // ── 制御点の追加 ─────────────────────────────────────

        /// tip（刃先）と root（柄）の 2 端点を追加する
        void AddPoint(const Vector3& tip, const Vector3& root);

        /// 幅方向ベクトルを明示して追加する（Arc/Fan 形状用）
        void AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir);

        // ── 毎フレーム ────────────────────────────────────────

        /// 経過時間を進め、TrailMesh を更新して CB を書き込む
        void Update(float deltaTime);

        /// デフォルトのコマンドリストで描画する（Framework から取得）
        void Draw();

        /// 明示的なコマンドリストで描画する
        void Draw(ID3D12GraphicsCommandList* cmdList);

        // ── 再生制御 ─────────────────────────────────────────

        /// トレイルの点の蓄積を開始する
        void Play();

        /// トレイルの点の蓄積を停止する（既存の点は寿命まで描画を続ける）
        void Stop();

        // ── アクセサ ─────────────────────────────────────────

        YoRigine::TrailMesh*            GetTrailMesh() const { return trailMesh_.get(); }
        const YoRigine::VfxEffectAsset& GetAsset()     const { return asset_; }

        /// 描画に使うカメラをセットする
        void SetCamera(YoRigine::Camera* camera) { camera_ = camera; }

    private:
        /// CB リソースが未確保なら確保して Map する
        void EnsureCB();

        /// TrailEffectParam の内容を CB に書き込む
        void UpdateCB();

    private:
        YoRigine::VfxEffectAsset             asset_;
        std::unique_ptr<YoRigine::TrailMesh> trailMesh_;
        bool isPlaying_ = false;

        YoRigine::Camera* camera_ = nullptr;
        float   time_   = 0.0f;

        Microsoft::WRL::ComPtr<ID3D12Resource> trailCBResource_;
        MeshTrailParamsCB* trailCBMapped_ = nullptr;
    };

} // namespace YoRigine
