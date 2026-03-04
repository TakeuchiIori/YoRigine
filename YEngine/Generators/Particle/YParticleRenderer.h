#pragma once
#include "DirectX/SrvManager.h"
#include "YParticleSystem.h"
#include "YParticleStructs.h"
#include "Systems/Camera/Camera.h"


#include "Material/MaterialColor.h"
#include "Material/MaterialLighting.h"
#include "Material/MaterialUV.h"
#include <Mesh/Mesh.h>

struct TransformForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

class YParticleRenderer {
public:
    void Initialize(SrvManager* srvManager, uint32_t maxTotalParticles = 10000);

    // フレーム開始時にリセット
    void BeginFrame();

    void ApplyLightSetting(const ParticleLightSetting& setting);

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

    SrvManager* srvManager_ = nullptr;
    YoRigine::DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

    // 現在のバッチに適用するライト設定
    ParticleLightSetting currentLightSetting_;

    // エンジンの既存マテリアル機能をコンポーネントとして保持
    std::unique_ptr<MaterialColor> materialColor_;
    std::unique_ptr<MaterialLighting> materialLighting_;
    std::unique_ptr<MaterialUV> materialUV_;
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
