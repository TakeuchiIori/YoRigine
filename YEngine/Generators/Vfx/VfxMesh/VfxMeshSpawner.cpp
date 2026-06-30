#include "VfxMeshSpawner.h"
#include "DirectXCommon.h"
#include "PipelineManager/YPipelineManager.h"
#include "Systems/Camera/Camera.h"
#include "Debugger/Logger.h"

#include <filesystem>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

static constexpr size_t kCBAlign = 256;
template<typename T>
static constexpr size_t CBSize() { return (sizeof(T) + kCBAlign - 1) & ~(kCBAlign - 1); }

// ============================================================
// シングルトン
// ============================================================
VfxMeshSpawner* VfxMeshSpawner::GetInstance()
{
    static VfxMeshSpawner inst;
    return &inst;
}

// ============================================================
// 初期化 / 終了
// ============================================================
void VfxMeshSpawner::Initialize()
{
    dxCommon_ = YoRigine::DirectXCommon::GetInstance();
}

void VfxMeshSpawner::Finalize()
{
    for (auto& fx : effects_) {
        if (fx) fx->Release();
    }
    effects_.clear();
    assetMap_.clear();
}

// ============================================================
// アセット登録
// ============================================================
void VfxMeshSpawner::LoadAsset(const std::string& filePath)
{
    YoRigine::VfxEffectAsset asset;
    if (!asset.LoadFromJson(filePath)) {
        Logger("VfxMeshSpawner: ロード失敗 -> " + filePath);
        return;
    }
    assetMap_[asset.name] = std::move(asset);
}

void VfxMeshSpawner::ScanDirectory(const std::string& dir)
{
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".json") continue;
        LoadAsset(entry.path().string());
    }
}

// ============================================================
// 生成
// ============================================================
uint32_t VfxMeshSpawner::Spawn(const std::string& assetName,
                               const Vector3&     position,
                               float              scale,
                               bool               loop)
{
    auto it = assetMap_.find(assetName);
    if (it == assetMap_.end()) {
        Logger("VfxMeshSpawner: アセット未登録 -> " + assetName);
        return 0;
    }

    auto fx       = std::make_unique<ActiveEffect>();
    fx->id        = nextId_++;
    fx->loop      = loop;
    fx->asset     = it->second;
    fx->position  = position;
    fx->scale     = scale;
    fx->burstProgress = loop ? -1.f : 0.f;

    // デフォルトの稲妻始終点（位置基準の上下）
    float half = fx->asset.lightning.length * 0.5f * scale;
    fx->boltStart = { position.x, position.y - half, position.z };
    fx->boltEnd   = { position.x, position.y + half, position.z };

    fx->smokeCenter = position;
    fx->smokeRadius = fx->asset.smoke.radius * scale;

    InitEffect(*fx);

    uint32_t id = fx->id;
    effects_.push_back(std::move(fx));
    return id;
}

uint32_t VfxMeshSpawner::SpawnBolt(const std::string& assetName,
                                   const Vector3&     start,
                                   const Vector3&     end,
                                   bool               loop)
{
    uint32_t id = Spawn(assetName, start, 1.0f, loop);
    if (id != 0) SetEndpoints(id, start, end);
    return id;
}

// ============================================================
// 初期化ヘルパ
// ============================================================
void VfxMeshSpawner::InitEffect(ActiveEffect& fx)
{
    const auto& asset = fx.asset;

    if (asset.useSmoke && asset.smoke.isEnable) {
        fx.smoke = std::make_unique<YoRigine::VolumeSmokeMesh>();
        fx.smoke->Initialize();
        fx.smoke->SetTransform(fx.position, fx.asset.smoke.radius * fx.scale);

        fx.smokeCBRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::SmokeParamsCB>());
        fx.smokeCBRes->Map(0, nullptr, reinterpret_cast<void**>(&fx.smokeCBMapped));
    }

    if (asset.useLightning && asset.lightning.isEnable) {
        fx.lightning = std::make_unique<YoRigine::LightningMesh>();
        fx.lightning->Initialize();
        fx.lightning->SetCamera(camera_);
        fx.lightning->ApplyParam(asset.lightning);
        fx.lightning->SetEndpoints(fx.boltStart, fx.boltEnd);

        fx.lightningCBRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::LightningParamsCB>());
        fx.lightningCBRes->Map(0, nullptr, reinterpret_cast<void**>(&fx.lightningCBMapped));
    }

    if (asset.useShockwave && asset.shockwave.isEnable) {
        fx.shockwave = std::make_unique<YoRigine::ShockwaveMesh>();
        fx.shockwave->Initialize();
        fx.shockwave->SetCamera(camera_);
        fx.shockwave->ApplyParam(asset.shockwave);
        fx.shockwave->SetTransform(fx.position, asset.shockwave.radius * fx.scale);

        fx.shockwaveCBRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::ShockwaveParamsCB>());
        fx.shockwaveCBRes->Map(0, nullptr, reinterpret_cast<void**>(&fx.shockwaveCBMapped));
    }
}

// ============================================================
// 毎フレーム更新
// ============================================================
void VfxMeshSpawner::Update(float deltaTime)
{
    for (auto& fx : effects_) {
        if (!fx || !fx->alive) continue;
        UpdateEffect(*fx, deltaTime);
    }

    // 死亡エフェクトを削除
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
            [](const std::unique_ptr<ActiveEffect>& f) {
                return !f || !f->alive;
            }),
        effects_.end());
}

void VfxMeshSpawner::UpdateEffect(ActiveEffect& fx, float dt)
{
    fx.age += dt;
    const auto& asset = fx.asset;

    // バースト進捗更新（ワンショット）
    if (!fx.loop) {
        float duration = std::max(asset.smoke.isEnable
            ? (asset.shockwave.isEnable
                ? std::max(asset.shockwave.duration, 2.0f)
                : 2.0f)
            : asset.shockwave.duration,
            0.1f);

        fx.burstProgress = std::min(fx.age / duration, 1.0f);
        if (fx.burstProgress >= 1.0f) {
            fx.alive = false;
            fx.Release();
            return;
        }
    }

    // Smoke 更新
    if (fx.smoke) {
        float rad = asset.smoke.radius * fx.scale;
        fx.smokeCenter = fx.position;
        if (fx.burstProgress >= 0.f) {
            float grow = std::min(fx.burstProgress / 0.18f, 1.0f);
            rad *= (0.2f + 0.8f * grow + 0.4f * fx.burstProgress);
            fx.smokeCenter.y += asset.smoke.riseSpeed * fx.age;
        }
        fx.smokeRadius = rad;
        fx.smoke->SetTransform(fx.smokeCenter, rad);
        fx.smoke->Update(dt);
    }

    // Lightning 更新
    if (fx.lightning) {
        fx.lightning->SetCamera(camera_);
        fx.lightning->SetEndpoints(fx.boltStart, fx.boltEnd);
        fx.lightning->Update(dt);
    }

    // Shockwave 更新
    if (fx.shockwave) {
        fx.shockwave->SetCamera(camera_);
        fx.shockwave->SetTransform(fx.position, asset.shockwave.radius * fx.scale);
        fx.shockwave->Update(dt);
    }
}

// ============================================================
// 描画
// ============================================================
void VfxMeshSpawner::Draw()
{
    if (!camera_) return;

    for (auto& fx : effects_) {
        if (!fx || !fx->alive) continue;
        DrawEffect(*fx);
    }
}

void VfxMeshSpawner::DrawEffect(ActiveEffect& fx)
{
    auto* pm      = YPipelineManager::GetInstance();
    auto* cmdList = dxCommon_->GetCommandList().Get();
    D3D12_GPU_VIRTUAL_ADDRESS camAddr = camera_->GetCameraResource()->GetGPUVirtualAddress();

    // Smoke
    if (fx.smoke && fx.smokeCBMapped) {
        const auto& sm = fx.asset.smoke;
        auto& cb = *fx.smokeCBMapped;
        cb.color         = sm.color;
        cb.smokeColor    = sm.smokeColor;
        cb.center        = fx.smokeCenter;
        cb.radius        = fx.smokeRadius;
        cb.time          = fx.age;
        cb.noiseScale    = sm.noiseScale;
        cb.noiseStrength = sm.noiseStrength;
        cb.scrollSpeed   = sm.scrollSpeed;
        cb.fresnelPower  = sm.fresnelPower;
        cb.density       = sm.density;
        cb.noiseOctaves  = sm.noiseOctaves;
        cb.rimIntensity  = sm.rimIntensity;
        cb.burst         = fx.burstProgress;

        const auto& idx = pm->GetParameterIndices("VfxMeshSmoke");
        cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshSmoke"));
        cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshSmoke"));
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), fx.smokeCBRes->GetGPUVirtualAddress());
        fx.smoke->Draw(cmdList);
    }

    // Lightning
    if (fx.lightning && fx.lightningCBMapped) {
        const auto& lt = fx.asset.lightning;
        auto& cb = *fx.lightningCBMapped;
        cb.color      = lt.color;
        cb.time       = fx.age;
        cb.glowPower  = lt.glowPower;
        cb.coreWidthN = 0.f;
        cb._pad       = 0.f;

        const auto& idx = pm->GetParameterIndices("VfxMeshLightning");
        cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshLightning"));
        cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshLightning"));
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), fx.lightningCBRes->GetGPUVirtualAddress());
        fx.lightning->Draw(cmdList);
    }

    // Shockwave
    if (fx.shockwave && fx.shockwaveCBMapped) {
        const auto& sw = fx.asset.shockwave;
        auto& cb = *fx.shockwaveCBMapped;
        cb.color     = sw.color;
        cb.time      = fx.age;
        cb.duration  = sw.duration;
        cb.thickness = sw.thickness;
        cb.burst     = (fx.burstProgress >= 0.f)
            ? std::min(fx.age / std::max(sw.duration, 0.01f), 1.0f)
            : -1.f;

        const auto& idx = pm->GetParameterIndices("VfxMeshShockwave");
        cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshShockwave"));
        cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshShockwave"));
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), fx.shockwaveCBRes->GetGPUVirtualAddress());
        fx.shockwave->Draw(cmdList);
    }
}

// ============================================================
// 操作
// ============================================================
void VfxMeshSpawner::SetPosition(uint32_t id, const Vector3& pos)
{
    for (auto& fx : effects_) {
        if (fx && fx->id == id) {
            fx->position    = pos;
            fx->smokeCenter = pos;
            return;
        }
    }
}

void VfxMeshSpawner::SetScale(uint32_t id, float scale)
{
    for (auto& fx : effects_) {
        if (fx && fx->id == id) {
            fx->scale = scale;
            return;
        }
    }
}

void VfxMeshSpawner::SetEndpoints(uint32_t id, const Vector3& start, const Vector3& end)
{
    for (auto& fx : effects_) {
        if (fx && fx->id == id) {
            fx->boltStart = start;
            fx->boltEnd   = end;
            return;
        }
    }
}

void VfxMeshSpawner::Stop(uint32_t id)
{
    for (auto& fx : effects_) {
        if (fx && fx->id == id) {
            fx->alive = false;
            fx->Release();
            return;
        }
    }
}

bool VfxMeshSpawner::IsAlive(uint32_t id) const
{
    for (const auto& fx : effects_) {
        if (fx && fx->id == id) return fx->alive;
    }
    return false;
}

// ============================================================
// ActiveEffect リソース解放
// ============================================================
void VfxMeshSpawner::ActiveEffect::Release()
{
    if (smokeCBMapped    && smokeCBRes)    { smokeCBRes->Unmap(0, nullptr);    smokeCBMapped    = nullptr; }
    if (lightningCBMapped && lightningCBRes) { lightningCBRes->Unmap(0, nullptr); lightningCBMapped = nullptr; }
    if (shockwaveCBMapped && shockwaveCBRes) { shockwaveCBRes->Unmap(0, nullptr); shockwaveCBMapped = nullptr; }
    smoke.reset();
    lightning.reset();
    shockwave.reset();
}
