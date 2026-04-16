#include "TrailMeshEmitter.h"

#include "DirectXCommon.h"
#include <PipelineManager/YPipelineManager.h>
#include <Loaders/Texture/TextureManager.h>
#include <SrvManager.h>

namespace YoRigine {

    namespace {
        const std::string kFallbackWhite = "Resources/Textures/white.png";

        // CBVの256バイトアライメント計算用
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

    void TrailMeshEmitter::UpdateCB() {
        if (!trailCBMapped_) return;

        const auto& t = asset_.trail;
        trailCBMapped_->colorInner[0] = t.colorEnd.x;
        trailCBMapped_->colorInner[1] = t.colorEnd.y;
        trailCBMapped_->colorInner[2] = t.colorEnd.z;
        trailCBMapped_->colorInner[3] = t.colorEnd.w;

        trailCBMapped_->colorOuter[0] = t.colorStart.x;
        trailCBMapped_->colorOuter[1] = t.colorStart.y;
        trailCBMapped_->colorOuter[2] = t.colorStart.z;
        trailCBMapped_->colorOuter[3] = t.colorStart.w;

        trailCBMapped_->softness = 0.15f;
        trailCBMapped_->glowPower = 1.5f;
        trailCBMapped_->distortion = 0.0f;
        trailCBMapped_->time = time_;
    }

    bool TrailMeshEmitter::LoadAsset(const std::string& filePath) {
        if (!asset_.LoadFromJson(filePath)) return false;

        if (!trailMesh_) trailMesh_ = std::make_unique<YoRigine::TrailMesh>();
        trailMesh_->Initialize(asset_.trail);
        trailMesh_->Clear();

        // テクスチャロードの保証
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

    void TrailMeshEmitter::SetAsset(const YoRigine::VfxEffectAsset& asset) {
        asset_ = asset;
        if (trailMesh_) {
            trailMesh_->ApplyParam(asset_.trail);
        }
        EnsureCB();
        UpdateCB();
    }

    void TrailMeshEmitter::AddPoint(const Vector3& tip, const Vector3& root) {
        if (!isPlaying_ || !trailMesh_) return;
        trailMesh_->AddPoint(tip, root);
    }

    void TrailMeshEmitter::AddPoint(const Vector3& tip, const Vector3& root, const Vector3& widthDir) {
        if (!isPlaying_ || !trailMesh_) return;
        trailMesh_->AddPoint(tip, root, widthDir);
    }

    void TrailMeshEmitter::Update(float deltaTime) {
        if (!isPlaying_ || !trailMesh_) return;
        time_ += deltaTime;
        trailMesh_->Update(deltaTime);
        UpdateCB();
    }

    void TrailMeshEmitter::Draw() {
        auto* cmd = YoRigine::DirectXCommon::GetInstance()->GetCommandList().Get();
        Draw(cmd);
    }

    void TrailMeshEmitter::Draw(ID3D12GraphicsCommandList* cmdList) {
        if (!isPlaying_ || !trailMesh_) return;
        if (!camera_) return;
        if (!trailCBResource_) return;

        // ===== 重要：SRV DescriptorHeap を必ずセット =====
        SrvManager::GetInstance()->PreDraw();

        auto* pm = YPipelineManager::GetInstance();
        const auto& idx = pm->GetParameterIndices("VfxMeshTrail");
        auto* texMgr = TextureManager::GetInstance();

        // SRVハンドル取得
        D3D12_GPU_DESCRIPTOR_HANDLE hNoise =
            (!asset_.trail.noiseTexturePath.empty())
            ? texMgr->GetsrvHandleGPU(asset_.trail.noiseTexturePath)
            : texMgr->GetsrvHandleGPU(kFallbackWhite);

        D3D12_GPU_DESCRIPTOR_HANDLE hRamp =
            (!asset_.trail.texturePath.empty())
            ? texMgr->GetsrvHandleGPU(asset_.trail.texturePath)
            : texMgr->GetsrvHandleGPU(kFallbackWhite);

        // PSO / RootSignature / RootParams
        cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshTrail"));
        cmdList->SetPipelineState(pm->GetBlendModePSO("VfxMeshTrail", asset_.trail.blendMode));

        cmdList->SetGraphicsRootConstantBufferView(
            idx.at("gCamera"),
            camera_->GetCameraResource()->GetGPUVirtualAddress()
        );
        cmdList->SetGraphicsRootConstantBufferView(
            idx.at("gMeshParam"),
            trailCBResource_->GetGPUVirtualAddress()
        );
        cmdList->SetGraphicsRootDescriptorTable(idx.at("gTexNoise"), hNoise); // t0
        cmdList->SetGraphicsRootDescriptorTable(idx.at("gTexRamp"), hRamp);   // t1

        // 描画
        trailMesh_->Draw(cmdList);
    }

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