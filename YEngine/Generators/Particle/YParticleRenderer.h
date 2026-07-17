#pragma once
#include "DirectX/SrvManager.h"
#include "YParticleSystem.h"
#include "YParticleStructs.h"
#include "Systems/Camera/Camera.h"


#include "Material/MaterialLighting.h"

#include <Mesh/Mesh.h>

struct TransformForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
	Matrix4x4 uvTransform;
};

class YParticleRenderer {
public:
    void Initialize(YoRigine::SrvManager* srvManager, uint32_t maxTotalParticles = 10000);

    // フレーム開始時にリセット
    void BeginFrame();

    void ApplyLightSetting(const ParticleLightSetting& setting);

    // 次に EndFrame するバッチのソフトパーティクル設定を指定する。
    // enabled=true のバッチは深度フェード用の専用 PSO（YParticleSoft）で描画される。
    void ApplySoftParticle(bool enabled, float fadeDistance);

    // システムをバッチに追加（まだ描画しない）
    void AddSystem(const YParticleSystem& system, Camera* camera);

    // 溜まったデータを一気に描画
    void EndFrame(const std::shared_ptr<Mesh>& mesh, uint32_t textureIndex, BlendMode blendMode);

    void CommitBatch();

    void ResetBatchOffset();

public:

    // ビルボード設定
    void SetBillboard(bool enable) { enableBillboard_ = enable; }
    bool IsBillboardEnabled() const { return enableBillboard_; }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    TransformForGPU* mappedData_ = nullptr;
    uint32_t srvIndex_;
    uint32_t maxTotalParticles_;
    uint32_t currentInstanceOffset_ = 0;
    uint32_t  batchStartOffset_ = 0;

    YoRigine::SrvManager* srvManager_ = nullptr;
    YoRigine::DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

    // 現在のバッチに適用するライト設定
    ParticleLightSetting currentLightSetting_;

    // 現在のバッチに適用するソフトパーティクル設定
    bool  currentSoftEnabled_ = false;
    float currentSoftFade_ = 0.5f;

    // ソフトパーティクル用 定数バッファ（バッチごとに 256B スロット）
    Microsoft::WRL::ComPtr<ID3D12Resource> softParticleCB_;
    struct SoftParticleCBData {
        float nearZ;
        float farZ;
        float fadeDistance;
        float padding;
    };
    uint8_t* mappedSoftCBBase_ = nullptr;

    std::unique_ptr<MaterialLighting> materialLighting_;

    bool enableBillboard_ = true;   // ビルボード有効/無効


    std::vector<uint32_t> batchEndOffsets_;
    uint32_t batchDrawIndex_ = 0;

private:
    // インスタンスオフセット用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceOffsetCB_;
    struct InstanceOffsetData {
        uint32_t offset;
        float padding[3];
    };
    static constexpr uint32_t MAX_BATCHES = 32;

    uint8_t* mappedOffsetBase_ = nullptr;  // uint8_t*にして手動でオフセット計算
    uint32_t currentBatchIndex_ = 0;
    uint32_t currentBatchStartOffset_ = 0;
};
