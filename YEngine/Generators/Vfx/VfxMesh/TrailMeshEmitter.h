#pragma once
#include <memory>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "VfxEffectAsset.h"
#include "TrailMesh.h"
#include "Systems/Camera/Camera.h"

namespace YoRigine {

    // Emitterが自前で管理する定数バッファの構造体
    struct MeshTrailParamsCB
    {
        float colorInner[4];
        float colorOuter[4];
        float softness;
        float glowPower;
        float distortion;
        float time;
    };

    class TrailMeshEmitter
    {
    public:
        TrailMeshEmitter() = default;
        ~TrailMeshEmitter();

        // アセットのロードと反映
        bool LoadAsset(const std::string& filePath);

        // Editorからのリアルタイム反映用
        void SetAsset(const YoRigine::VfxEffectAsset& asset);

        // ポイント追加
        void AddPoint(const Vector3& tip, const Vector3& root);
        void AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir);

        void Update(float deltaTime);

        // Draw を完結させる：呼び出し側は cmdList を渡さなくて良い
        void Draw();
        // 既存互換
        void Draw(ID3D12GraphicsCommandList* cmdList);

        // 再生・停止
        void Play();
        void Stop();

    public:
        YoRigine::TrailMesh* GetTrailMesh() const { return trailMesh_.get(); }
        const YoRigine::VfxEffectAsset& GetAsset() const { return asset_; }
        void SetCamera(Camera* camera) { camera_ = camera; }

    private:
        void EnsureCB();
        void UpdateCB();

    private:
        YoRigine::VfxEffectAsset asset_;
        std::unique_ptr<YoRigine::TrailMesh> trailMesh_;
        bool isPlaying_ = false;

        Camera* camera_ = nullptr;
        float time_ = 0.0f; // シェーダーに渡す時間

        // 定数バッファ
        Microsoft::WRL::ComPtr<ID3D12Resource> trailCBResource_;
        MeshTrailParamsCB* trailCBMapped_ = nullptr;
    };

} // namespace YoRigine