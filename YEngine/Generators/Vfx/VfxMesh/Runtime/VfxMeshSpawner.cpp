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

    // 稲妻の端点は既定では各サブ効果の direction/length から自動計算する。
    // SetEndpoints / SpawnBolt で明示指定された場合のみ boltStart/End を使う。
    fx->explicitEndpoints = false;
    fx->boltStart = position;
    fx->boltEnd   = position;

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

    fx.subs.clear();
    fx.subs.reserve(asset.subEffects.size());

    for (const auto& def : asset.subEffects) {
        if (!def.enabled) continue;

        SubEffectRT sub;
        sub.def = &def;
        const Vector3 basePos = fx.position + def.offset * fx.scale;

        switch (def.type) {
        case YoRigine::VfxSubEffectType::Smoke:
            sub.smoke = std::make_unique<YoRigine::VolumeSmokeMesh>();
            sub.smoke->Initialize();
            sub.smoke->ApplyParam(def.smoke);   // 半径/上昇速度を Drive で使えるように
            sub.smoke->SetTransform(basePos, def.smoke.radius * fx.scale);
            sub.smokeCenter = basePos;
            sub.smokeRadius = def.smoke.radius * fx.scale;

            sub.cbRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::SmokeParamsCB>());
            sub.cbRes->Map(0, nullptr, &sub.cbMapped);
            break;

        case YoRigine::VfxSubEffectType::Lightning:
            sub.lightning = std::make_unique<YoRigine::LightningMesh>();
            sub.lightning->Initialize();
            sub.lightning->SetCamera(camera_);
            sub.lightning->ApplyParam(def.lightning);

            sub.cbRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::LightningParamsCB>());
            sub.cbRes->Map(0, nullptr, &sub.cbMapped);
            break;

        case YoRigine::VfxSubEffectType::Shockwave:
            sub.shockwave = std::make_unique<YoRigine::ShockwaveMesh>();
            sub.shockwave->Initialize();
            sub.shockwave->SetCamera(camera_);
            sub.shockwave->ApplyParam(def.shockwave);
            sub.shockwave->SetTransform(basePos, def.shockwave.radius * fx.scale);

            sub.cbRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::ShockwaveParamsCB>());
            sub.cbRes->Map(0, nullptr, &sub.cbMapped);
            break;

        case YoRigine::VfxSubEffectType::LightVolume:
            sub.lightVolume = std::make_unique<YoRigine::LightVolumeMesh>();
            sub.lightVolume->Initialize(def.lightVolume);
            sub.lightVolume->SetTransform(basePos, 0.0f);

            sub.cbRes = dxCommon_->CreateBufferResource(CBSize<YoRigine::LightVolumeParamsCB>());
            sub.cbRes->Map(0, nullptr, &sub.cbMapped);
            break;

        default:
            continue;
        }

        fx.subs.push_back(std::move(sub));
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

    // バースト進捗更新（ワンショット）。寿命はモーション(BurstGrow)優先で決定。
    float lifetime = -1.f;
    if (!fx.loop) {
        lifetime = asset.OneShotDuration();
        fx.burstProgress = std::min(fx.age / lifetime, 1.0f);
        if (fx.burstProgress >= 1.0f) {
            fx.alive = false;
            fx.Release();
            return;
        }
    }

    // 各 Mesh 共通のベース状態（動きの計算は Mesh 側 Drive に集約）
    YoRigine::VfxEvalState base;
    base.age       = fx.age;
    base.progress  = fx.burstProgress;
    base.lifetime  = lifetime;
    base.position  = fx.position;
    base.scale     = fx.scale;
    base.boltStart = fx.boltStart;
    base.boltEnd   = fx.boltEnd;

    // サブ効果ごとに: オフセット → モーション（全体＋個別） → Drive
    for (auto& sub : fx.subs) {
        const auto& def = *sub.def;

        YoRigine::VfxEvalState s = base;
        const Vector3 offset = def.offset * fx.scale;
        s.position  += offset;
        s.boltStart += offset;
        s.boltEnd   += offset;
        YoRigine::EvaluateSubEffectMotions(asset, def, s);

        // 色乗算・表示状態は Draw の CB 反映で使う
        sub.tint    = s.colorTint;
        sub.visible = s.visible;

        switch (def.type) {
        case YoRigine::VfxSubEffectType::Smoke:
            if (sub.smoke) {
                sub.smoke->Drive(s);
                sub.smoke->Update(dt);
                sub.smokeCenter = sub.smoke->GetCenter();  // CB 用に読み戻す
                sub.smokeRadius = sub.smoke->GetRadius();
            }
            break;

        case YoRigine::VfxSubEffectType::Lightning:
            if (sub.lightning) {
                // 端点が明示指定されていなければ direction/length から自動計算
                // （モーション適用後の中心を挟んで direction 方向へ伸ばす）
                if (!fx.explicitEndpoints) {
                    Vector3 dir = def.lightning.direction;
                    const float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                    dir = (dl < 1e-4f) ? Vector3{ 0.f, 1.f, 0.f } : dir / dl;
                    const float half = def.lightning.length * 0.5f * fx.scale;
                    s.boltStart = s.position - dir * half;
                    s.boltEnd   = s.position + dir * half;
                }
                sub.lightning->SetCamera(camera_);
                sub.lightning->Drive(s);
                sub.lightning->Update(dt);
            }
            break;

        case YoRigine::VfxSubEffectType::Shockwave:
            if (sub.shockwave) {
                sub.shockwave->SetCamera(camera_);
                sub.shockwave->Drive(s);
                sub.shockwave->Update(dt);
            }
            break;

        case YoRigine::VfxSubEffectType::LightVolume:
            if (sub.lightVolume) {
                sub.lightVolume->ApplyParam(def.lightVolume);
                sub.lightVolume->SetTransform(s.position, 0.0f); // Y軸回転はまだ使わない
                sub.lightVolume->Update(dt);
            }
            break;

        default:
            break;
        }
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

    // モーションの色乗算（rgb=色 / a=不透明度）を CB 用カラーへ適用する
    auto tint4 = [](const Vector4& c, const Vector4& t) -> Vector4 {
        return { c.x * t.x, c.y * t.y, c.z * t.z, c.w * t.w };
    };

    for (auto& sub : fx.subs) {
        if (!sub.cbMapped || !sub.cbRes) continue;
        if (!sub.visible || sub.tint.w <= 0.001f) continue; // Visibility/フェードで消えている
        const auto& def = *sub.def;

        switch (def.type) {
        case YoRigine::VfxSubEffectType::Smoke:
            if (sub.smoke) {
                const auto& sm = def.smoke;
                auto& cb = *static_cast<YoRigine::SmokeParamsCB*>(sub.cbMapped);
                cb.color         = tint4(sm.color, sub.tint);
                cb.smokeColor    = tint4(sm.smokeColor, sub.tint);
                cb.center        = sub.smokeCenter;
                cb.radius        = sub.smokeRadius;
                cb.time          = fx.age;
                cb.noiseScale    = sm.noiseScale;
                cb.noiseStrength = sm.noiseStrength;
                cb.scrollSpeed   = sm.scrollSpeed;
                cb.fresnelPower  = sm.fresnelPower;
                cb.density       = sm.density;
                cb.noiseOctaves  = sm.noiseOctaves;
                cb.rimIntensity  = sm.rimIntensity;
                // Smoke の色/フェード/膨張/上昇はモーション駆動。シェーダは burst を使わないので -1（未使用）。
                cb.burst         = -1.f;

                const auto& idx = pm->GetParameterIndices("VfxMeshSmoke");
                cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshSmoke"));
                cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshSmoke"));
                cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
                cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                sub.smoke->Draw(cmdList);
            }
            break;

        case YoRigine::VfxSubEffectType::Lightning:
            if (sub.lightning) {
                const auto& lt = def.lightning;
                auto& cb = *static_cast<YoRigine::LightningParamsCB*>(sub.cbMapped);
                cb.color            = tint4(lt.color, sub.tint);
                cb.glowColor        = tint4(lt.glowColor, sub.tint);
                cb.branchColor      = tint4(lt.branchColor, sub.tint);
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
                cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                sub.lightning->Draw(cmdList);
            }
            break;

        case YoRigine::VfxSubEffectType::Shockwave:
            if (sub.shockwave) {
                const auto& sw = def.shockwave;
                auto& cb = *static_cast<YoRigine::ShockwaveParamsCB*>(sub.cbMapped);
                cb.color     = tint4(sw.color, sub.tint);
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
                cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                sub.shockwave->Draw(cmdList);
            }
            break;

        case YoRigine::VfxSubEffectType::LightVolume:
            if (sub.lightVolume) {
                const auto& lv = def.lightVolume;
                auto& cb = *static_cast<YoRigine::LightVolumeParamsCB*>(sub.cbMapped);
                // Editor プレビュー(VfxMeshEditor)の LightVolume と同じ CB 構成
                cb.color[0] = lv.color.x * sub.tint.x;
                cb.color[1] = lv.color.y * sub.tint.y;
                cb.color[2] = lv.color.z * sub.tint.z;
                cb.color[3] = lv.color.w * lv.intensity * sub.tint.w;
                cb.edgeFade      = lv.edgeFade;
                cb.depthFade     = lv.depthFade;
                cb.noiseTiling   = lv.noiseTiling;
                cb.noiseStrength = lv.noiseStrength;
                cb.time          = fx.age;
                cb.beamStrength  = lv.beamStrength;
                cb.beamRadius    = lv.beamRadius;
                cb.beamPower     = lv.beamPower;
                cb.beamGlow      = lv.beamGlow;

                const auto& idx = pm->GetParameterIndices("VfxMeshVolume");
                cmdList->SetGraphicsRootSignature(pm->GetRootSignature("VfxMeshVolume"));
                cmdList->SetPipelineState(pm->GetPipeLineStateObject("VfxMeshVolume"));
                cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
                cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
                sub.lightVolume->Draw(cmdList);
            }
            break;

        default:
            break;
        }
    }
}

// ============================================================
// 操作
// ============================================================
void VfxMeshSpawner::SetPosition(uint32_t id, const Vector3& pos)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->position = pos;
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
            fx->explicitEndpoints = true;
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
    for (auto& sub : subs) {
        if (sub.cbMapped && sub.cbRes) { sub.cbRes->Unmap(0, nullptr); sub.cbMapped = nullptr; }
        sub.smoke.reset();
        sub.lightning.reset();
        sub.shockwave.reset();
    }
    subs.clear();
}
