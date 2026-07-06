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
    active_.reserve(kMaxActiveVfx);   // 以降 push_back で再確保させない
}

void VfxMeshSpawner::Finalize()
{
    // pool_.Clear() が生存中の ActiveEffect を破棄 → ~ActiveEffect が Release() を呼ぶ
    pool_.Clear();
    active_.clear();
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

std::vector<std::string> VfxMeshSpawner::GetAssetNames() const
{
    std::vector<std::string> names;
    names.reserve(assetMap_.size());
    for (const auto& [name, asset] : assetMap_) {
        names.push_back(name);
    }
    return names;
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

    ActiveEffect* fx = pool_.Alloc();
    if (!fx) {
        Logger("VfxMeshSpawner: プール満杯（同時上限 " + std::to_string(kMaxActiveVfx) + "）-> " + assetName);
        return 0;
    }
    fx->id        = nextId_++;
    fx->alive     = true;
    fx->loop      = loop;
    fx->asset     = &it->second;   // コピーせず参照
    fx->position  = position;
    fx->scale     = scale;
    fx->burstProgress = loop ? -1.f : 0.f;

    // デフォルトの稲妻始終点（位置基準の上下）
    float half = fx->asset->lightning.length * 0.5f * scale;
    fx->boltStart = { position.x, position.y - half, position.z };
    fx->boltEnd   = { position.x, position.y + half, position.z };

    fx->smokeCenter = position;
    fx->smokeRadius = fx->asset->smoke.radius * scale;

    InitEffect(*fx);

    active_.push_back(fx);
    return fx->id;
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
    const auto& asset = *fx.asset;

    if (asset.useSmoke && asset.smoke.isEnable) {
        fx.smoke = std::make_unique<YoRigine::VolumeSmokeMesh>();
        fx.smoke->Initialize();
        fx.smoke->ApplyParam(asset.smoke);   // 半径/上昇速度を Drive で使えるように
        fx.smoke->SetTransform(fx.position, asset.smoke.radius * fx.scale);

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
    for (size_t i = 0; i < active_.size(); ) {
        ActiveEffect* fx = active_[i];
        if (fx && fx->alive) UpdateEffect(*fx, deltaTime);

        if (!fx || !fx->alive) {
            // 死亡 → プールへ返却し、active_ から swap-remove（再確保なし）
            if (fx) pool_.Free(fx);
            active_[i] = active_.back();
            active_.pop_back();
            continue;
        }
        ++i;
    }
}

void VfxMeshSpawner::UpdateEffect(ActiveEffect& fx, float dt)
{
    fx.age += dt;
    const auto& asset = *fx.asset;

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

    // 各 Mesh へ渡す共有状態を組み立て（動きの計算は Mesh 側 Drive に集約）
    YoRigine::VfxEvalState st;
    st.age       = fx.age;
    st.progress  = fx.burstProgress;
    st.position  = fx.position;
    st.scale     = fx.scale;
    st.boltStart = fx.boltStart;
    st.boltEnd   = fx.boltEnd;

    // Smoke 更新（膨張/上昇は Drive 内で計算 → 結果を CB 用に読み戻す）
    if (fx.smoke) {
        fx.smoke->Drive(st);
        fx.smoke->Update(dt);
        fx.smokeCenter = fx.smoke->GetCenter();
        fx.smokeRadius = fx.smoke->GetRadius();
    }

    // Lightning 更新
    if (fx.lightning) {
        fx.lightning->SetCamera(camera_);
        fx.lightning->Drive(st);
        fx.lightning->Update(dt);
    }

    // Shockwave 更新
    if (fx.shockwave) {
        fx.shockwave->SetCamera(camera_);
        fx.shockwave->Drive(st);
        fx.shockwave->Update(dt);
    }
}

// ============================================================
// 描画
// ============================================================
void VfxMeshSpawner::Draw()
{
    if (!camera_) return;

    for (ActiveEffect* fx : active_) {
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
        const auto& sm = fx.asset->smoke;
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
        const auto& lt = fx.asset->lightning;
        auto& cb = *fx.lightningCBMapped;
        cb.color            = lt.color;
        cb.glowColor        = lt.glowColor;
        cb.branchColor      = lt.branchColor;
        cb.time             = fx.age;
        cb.glowPower        = lt.glowPower;
        cb.coreWidth        = lt.coreWidth;
        cb.solidness        = lt.solidness;
        cb.outlineIntensity = lt.outlineIntensity;
        cb._pad0 = cb._pad1 = cb._pad2 = 0.f;

        const auto& idx = pm->GetParameterIndices("VfxMeshLightning");
        cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshLightning"));
        cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshLightning"));
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), fx.lightningCBRes->GetGPUVirtualAddress());
        fx.lightning->Draw(cmdList);
    }

    // Shockwave
    if (fx.shockwave && fx.shockwaveCBMapped) {
        const auto& sw = fx.asset->shockwave;
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
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->position    = pos;
            fx->smokeCenter = pos;
            return;
        }
    }
}

void VfxMeshSpawner::SetScale(uint32_t id, float scale)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->scale = scale;
            return;
        }
    }
}

void VfxMeshSpawner::SetEndpoints(uint32_t id, const Vector3& start, const Vector3& end)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->boltStart = start;
            fx->boltEnd   = end;
            return;
        }
    }
}

void VfxMeshSpawner::Stop(uint32_t id)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->alive = false;
            fx->Release();
            return;
        }
    }
}

bool VfxMeshSpawner::IsAlive(uint32_t id) const
{
    for (const ActiveEffect* fx : active_) {
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
