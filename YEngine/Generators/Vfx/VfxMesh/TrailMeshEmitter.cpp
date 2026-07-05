// ===========================================================
// TrailMeshEmitter.cpp
// ===========================================================
#include "TrailMeshEmitter.h"

#include "DirectXCommon.h"
#include <PipelineManager/YPipelineManager.h>
#include <Loaders/Texture/TextureManager.h>
#include <SrvManager.h>

namespace YoRigine {

    namespace {
        const std::string kFallbackWhite = "Resources/Textures/white.png";

        static constexpr size_t kCBVAlignment = 256;
        template<typename T>
        static constexpr size_t AlignedSize() {
            return (sizeof(T) + kCBVAlignment - 1) & ~(kCBVAlignment - 1);
        }
    }

    TrailMeshEmitter::~TrailMeshEmitter() {
        if (trailCBMapped_ && trailCBResource_) {
            trailCBResource_->Unmap(0, nullptr);
            trailCBMapped_ = nullptr;
        }
    }

    void TrailMeshEmitter::EnsureCB() {
        if (trailCBResource_) return;

        auto* dxCommon = YoRigine::DirectXCommon::GetInstance();
        trailCBResource_ = dxCommon->CreateBufferResource(AlignedSize<MeshTrailParamsCB>());
        trailCBResource_->Map(0, nullptr, reinterpret_cast<void**>(&trailCBMapped_));
    }

    // -------------------------------------------------------
    // HLSL MeshTrailParams と完全に 1:1 対応させてマッピング
    // -------------------------------------------------------
    void TrailMeshEmitter::UpdateCB() {
        if (!trailCBMapped_) return;

        const auto& t = asset_.trail;

        // Row0: colorInner ← trail.colorEnd (先端 / x=0 側)
        trailCBMapped_->colorInner[0] = t.colorEnd.x;
        trailCBMapped_->colorInner[1] = t.colorEnd.y;
        trailCBMapped_->colorInner[2] = t.colorEnd.z;
        trailCBMapped_->colorInner[3] = t.colorEnd.w;

        // Row1: colorOuter ← trail.colorStart (根本 / x=1 側)
        trailCBMapped_->colorOuter[0] = t.colorStart.x;
        trailCBMapped_->colorOuter[1] = t.colorStart.y;
        trailCBMapped_->colorOuter[2] = t.colorStart.z;
        trailCBMapped_->colorOuter[3] = t.colorStart.w;

        // Row2: rimColor ★NEW
        trailCBMapped_->rimColor[0] = t.rimColor.x;
        trailCBMapped_->rimColor[1] = t.rimColor.y;
        trailCBMapped_->rimColor[2] = t.rimColor.z;
        trailCBMapped_->rimColor[3] = t.rimColor.w;

        // Row3: 基本制御
        trailCBMapped_->softness   = t.softness;
        trailCBMapped_->glowPower  = t.glowPower;
        trailCBMapped_->distortion = t.distortion;
        trailCBMapped_->time       = time_;

        // Row4: エネルギーライン / スパークル ★NEW
        trailCBMapped_->energyIntensity = t.energyIntensity;
        trailCBMapped_->energySpeed     = t.energySpeed;
        trailCBMapped_->sparkleAmount   = t.sparkleAmount;
        trailCBMapped_->sparkleSpeed    = t.sparkleSpeed;

        // Row5: フレネル / ウェーブ ★NEW
        trailCBMapped_->fresnelStrength = t.fresnelStrength;
        trailCBMapped_->trailSharpness  = t.trailSharpness;
        trailCBMapped_->colorWaveFreq   = t.colorWaveFreq;
        trailCBMapped_->colorWaveAmp    = t.colorWaveAmp;

        // Row6: UV / ノイズ / ディゾルブ ★NEW
        trailCBMapped_->uvScrollSpeed    = t.uvScrollSpeed;
        trailCBMapped_->noiseOctaves     = t.noiseOctaves;
        trailCBMapped_->dissolveStrength  = t.dissolveStrength;
        trailCBMapped_->dissolveEdgeWidth = t.dissolveEdgeWidth;

        // Row7: ディゾルブ エッジ色 ★NEW
        trailCBMapped_->dissolveEdgeColor[0] = t.dissolveEdgeColor.x;
        trailCBMapped_->dissolveEdgeColor[1] = t.dissolveEdgeColor.y;
        trailCBMapped_->dissolveEdgeColor[2] = t.dissolveEdgeColor.z;
        trailCBMapped_->dissolveEdgeColor[3] = t.dissolveEdgeColor.w;

        // Row8: 発光マスター強度 ★NEW
        trailCBMapped_->emissiveIntensity = t.emissiveIntensity;
        trailCBMapped_->_pad2 = 0.f;
        trailCBMapped_->_pad3 = 0.f;
        trailCBMapped_->_pad4 = 0.f;
    }

    // -------------------------------------------------------
    bool TrailMeshEmitter::LoadAsset(const std::string& filePath) {
        if (!asset_.LoadFromJson(filePath)) return false;

        if (!trailMesh_) trailMesh_ = std::make_unique<YoRigine::TrailMesh>();
        trailMesh_->Initialize(asset_.trail);
        trailMesh_->Clear();

        auto* texMgr = TextureManager::GetInstance();
        texMgr->LoadTexture(kFallbackWhite);

        if (!asset_.trail.noiseTexturePath.empty()) {
            texMgr->LoadTexture(asset_.trail.noiseTexturePath);
        }
        if (!asset_.trail.texturePath.empty()) {
            texMgr->LoadTexture(asset_.trail.texturePath);
        }

        EnsureCB();
        UpdateCB();
        return true;
    }

    // -------------------------------------------------------
    void TrailMeshEmitter::SetAsset(const YoRigine::VfxEffectAsset& asset) {
        asset_ = asset;
        if (trailMesh_) {
            trailMesh_->ApplyParam(asset_.trail);
        }
        EnsureCB();
        UpdateCB();
    }

    // -------------------------------------------------------
    void TrailMeshEmitter::AddPoint(const Vector3& tip, const Vector3& root) {
        if (!isPlaying_ || !trailMesh_) return;
        trailMesh_->AddPoint(tip, root);
    }

    void TrailMeshEmitter::AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir) {
        if (!isPlaying_ || !trailMesh_) return;
        trailMesh_->AddPoint(tip, root, widthDir);
    }

    // -------------------------------------------------------
    void TrailMeshEmitter::Update(float deltaTime) {
        if (!isPlaying_ || !trailMesh_) return;
        time_ += deltaTime;
        trailMesh_->Update(deltaTime);
        UpdateCB();
    }

    // -------------------------------------------------------
    void TrailMeshEmitter::Draw() {
        auto* cmd = YoRigine::DirectXCommon::GetInstance()->GetCommandList().Get();
        Draw(cmd);
    }

    void TrailMeshEmitter::Draw(ID3D12GraphicsCommandList* cmdList) {
        if (!isPlaying_ || !trailMesh_) return;
        if (!camera_)           return;
        if (!trailCBResource_)  return;

        SrvManager::GetInstance()->PreDraw();

        auto* pm     = YPipelineManager::GetInstance();
        auto* texMgr = TextureManager::GetInstance();
        const auto& idx = pm->GetParameterIndices("VfxMeshTrail");

        // ===== SRV ハンドル取得 (白テクスチャをデフォルトにして GPU Validation を回避) =====
        D3D12_GPU_DESCRIPTOR_HANDLE hNoise =
            (!asset_.trail.noiseTexturePath.empty())
            ? texMgr->GetsrvHandleGPU(asset_.trail.noiseTexturePath)
            : texMgr->GetsrvHandleGPU(kFallbackWhite);

        D3D12_GPU_DESCRIPTOR_HANDLE hRamp =
            (!asset_.trail.texturePath.empty())
            ? texMgr->GetsrvHandleGPU(asset_.trail.texturePath)
            : texMgr->GetsrvHandleGPU(kFallbackWhite);

        // ===== パイプライン設定 =====
        cmdList->SetGraphicsRootSignature(
            pm->GetRootSignature("VfxMeshTrail"));
        cmdList->SetPipelineState(
            pm->GetBlendModePSO("VfxMeshTrail", asset_.trail.blendMode));

        // b0: Camera CBV
        cmdList->SetGraphicsRootConstantBufferView(
            idx.at("gCamera"),
            camera_->GetCameraResource()->GetGPUVirtualAddress());

        // b1: MeshTrailParams CBV
        cmdList->SetGraphicsRootConstantBufferView(
            idx.at("gMeshParam"),
            trailCBResource_->GetGPUVirtualAddress());

        // t0: ノイズテクスチャ (未設定時は白テクスチャで GPU Validation エラー回避)
        cmdList->SetGraphicsRootDescriptorTable(
            idx.at("gTexNoise"), hNoise);

        // t1: ランプテクスチャ (同上)
        cmdList->SetGraphicsRootDescriptorTable(
            idx.at("gTexRamp"), hRamp);

        // ===== 描画 =====
        trailMesh_->Draw(cmdList);
    }

    // -------------------------------------------------------
    void TrailMeshEmitter::Play() {
        isPlaying_ = true;
        time_ = 0.0f;
        if (trailMesh_) trailMesh_->Clear();
    }

    void TrailMeshEmitter::Stop() {
        isPlaying_ = false;
        if (trailMesh_) trailMesh_->Clear();
    }

} // namespace YoRigine
