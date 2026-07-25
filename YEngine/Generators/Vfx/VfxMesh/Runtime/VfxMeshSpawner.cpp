#include "VfxMeshSpawner.h"
#include "DirectXCommon.h"
#include "PipelineManager/YPipelineManager.h"
#include "Systems/Camera/Camera.h"
#include "Debugger/Logger.h"

#include <Vfx/VfxMesh/Effects/LightningMesh.h>
#include <Vfx/VfxMesh/Effects/LightVolumeMesh.h>
#include <Vfx/VfxMesh/Effects/VolumeSmokeMesh.h>
#include <Vfx/VfxMesh/Effects/ShockwaveMesh.h>

// 2軸合成（Geometry × Material）
#include <Vfx/VfxMesh/Core/GeometryRegistry.h>
#include <Vfx/VfxMesh/Core/MaterialRegistry.h>
#include <Vfx/VfxMesh/Geometry/SphereGeometry.h>
#include <Vfx/VfxMesh/Geometry/ConeGeometry.h>
#include <Vfx/VfxMesh/Geometry/RingGeometry.h>
#include <Vfx/VfxMesh/Geometry/PlaneGeometry.h>
#include <Vfx/VfxMesh/Geometry/DiscGeometry.h>
#include <Vfx/VfxMesh/Geometry/RimCurtainGeometry.h>
#include <Vfx/VfxMesh/Geometry/LobeClusterGeometry.h>
#include <Vfx/VfxMesh/Materials/NoiseMaterial.h>
#include <Vfx/VfxMesh/Materials/ShockwaveMaterial.h>
#include <Vfx/VfxMesh/Materials/AreaFieldMaterial.h>
#include <Vfx/VfxMesh/Materials/RimFxMaterial.h>
#include <Vfx/VfxMesh/Core/VfxMeshElement.h>

#include <filesystem>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

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
    active_.reserve(kMaxActiveVfx);

    // ── 既知エフェクト型を Registry に登録（モノリシック / 従来型） ──────
    // 新しい型を追加するときは Describe() を書いて1行追加するだけ。
    auto& reg = YoRigine::VfxMeshRegistry::Instance();
    reg.Register(YoRigine::VolumeSmokeMesh::Describe());
    reg.Register(YoRigine::LightningMesh::Describe());
    reg.Register(YoRigine::ShockwaveMesh::Describe());
    reg.Register(YoRigine::LightVolumeMesh::Describe());

    // ── 2軸合成用のジオメトリ / マテリアルを登録 ───────────────────────
    // Geometry × Material の任意の組み合わせでエフェクトを作れる。
    auto& geomReg = YoRigine::GeometryRegistry::Instance();
    geomReg.Register(YoRigine::SphereGeometry::Describe());
    geomReg.Register(YoRigine::ConeGeometry::Describe());
    geomReg.Register(YoRigine::RingGeometry::Describe());
    geomReg.Register(YoRigine::PlaneGeometry::Describe());
    geomReg.Register(YoRigine::DiscGeometry::Describe());
    geomReg.Register(YoRigine::RimCurtainGeometry::Describe());
    geomReg.Register(YoRigine::LobeClusterGeometry::Describe());

    auto& matReg = YoRigine::MaterialRegistry::Instance();
    matReg.Register(YoRigine::NoiseMaterial::Describe());
    matReg.Register(YoRigine::ShockwaveMaterial::Describe());
    matReg.Register(YoRigine::AreaFieldMaterial::Describe());
    matReg.Register(YoRigine::RimFxMaterial::Describe());
}

void VfxMeshSpawner::Finalize()
{
    // pool_.Clear() が生存中の ActiveEffect を破棄 → ~ActiveEffect が Release() を呼ぶ
    pool_.Clear();
    active_.clear();
    assetMap_.clear();
}

void VfxMeshSpawner::StopAll()
{
    pool_.Clear();
    active_.clear();
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

const YoRigine::VfxEffectAsset* VfxMeshSpawner::GetAsset(const std::string& assetName) const
{
    auto it = assetMap_.find(assetName);
    return (it != assetMap_.end()) ? &it->second : nullptr;
}

// ============================================================
// 生成
// ============================================================
uint32_t VfxMeshSpawner::Spawn(const std::string& assetName,
                               const Vector3&     position,
                               float              scale,
                               bool               loop,
                               float              timeScale,
                               float              lifetime)
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
    fx->age       = 0.f;         // プール再利用時に前回の経過時間を持ち越さない
    fx->lifetimeOverride = (lifetime > 0.f) ? lifetime : -1.f;
    // アセット側で「寿命無限」が立っていれば loop を強制 true（呼び出し側が false でも消えない）。
    // ただし寿命(秒)が明示指定された場合はワンショット扱いとし、必ずその秒数で消す。
    const bool effLoop = (fx->lifetimeOverride > 0.f)
                             ? false
                             : (loop || it->second.loopForever);
    fx->loop      = effLoop;
    fx->timeScale = (timeScale > 0.0001f) ? timeScale : 1.0f;
    fx->asset     = &it->second;   // コピーせず参照
    fx->position  = position;
    fx->scale     = scale;
    fx->userColor = { 1.f, 1.f, 1.f, 1.f }; // プール再利用時に前回の色を持ち越さない
    fx->burstProgress = effLoop ? -1.f : 0.f;

    // 稲妻の端点は既定では各エレメントの direction/length から自動計算する。
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
                                   bool               loop,
                                   float              timeScale)
{
    uint32_t id = Spawn(assetName, start, 1.0f, loop, timeScale);
    if (id != 0) SetEndpoints(id, start, end);
    return id;
}

// ============================================================
// 初期化ヘルパ
// ============================================================
void VfxMeshSpawner::InitEffect(ActiveEffect& fx)
{
    fx.subs.clear();
    fx.subs.reserve(fx.asset->elements.size());

    auto& reg = YoRigine::VfxMeshRegistry::Instance();

    for (const auto& def : fx.asset->elements) {
        if (!def.enabled) continue;

        const Vector3 basePos = fx.position + def.offset * fx.scale;

        // Composed（Geometry × Material）と Monolithic（専用メッシュ）で生成経路を分ける
        std::unique_ptr<YoRigine::ProceduralMeshBase> mesh;
        if (def.kind == YoRigine::VfxElementKind::Composed) {
            mesh = YoRigine::VfxMeshElement::CreateComposed(
                def.GeometryType(), def.geom,
                def.MaterialType(), def.mat, camera_);
        } else {
            mesh = reg.Create(def, basePos, fx.scale, camera_);
        }
        if (!mesh) continue;

        ElementRT sub;
        sub.def  = &def;
        sub.mesh = std::move(mesh);
        sub.cbRes = dxCommon_->CreateBufferResource(sub.mesh->GetCBByteSize());
        sub.cbRes->Map(0, nullptr, &sub.cbMapped);

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
    // timeScale<1 で寿命を引き伸ばす（見た目のアニメ速度そのものを遅くする）。
    // 各Meshの Drive/Update には「進んだage」だけを渡すので、Mesh側の実装は変更不要。
    const float scaledDt = dt * fx.timeScale;
    fx.age += scaledDt;
    dt = scaledDt;
    const auto& asset = *fx.asset;

    // バースト進捗更新（ワンショット）。寿命は
    //   lifetimeOverride(秒) が指定されていれば最優先、
    //   無ければアセットの OneShotDuration（モジュール Lifetime 優先）で決定。
    float lifetime = -1.f;
    if (!fx.loop || fx.lifetimeOverride > 0.f) {
        lifetime = (fx.lifetimeOverride > 0.f) ? fx.lifetimeOverride
                                               : asset.OneShotDuration();
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

    // エレメントごとに: オフセット → モジュール（全体＋個別） → Drive → Update
    for (auto& sub : fx.subs) {
        const auto& def = *sub.def;

        YoRigine::VfxEvalState s = base;
        const Vector3 offset = def.offset * fx.scale;
        s.position  += offset;
        s.boltStart += offset;
        s.boltEnd   += offset;
        s.rotation   = def.rotation;   // エレメントの基準向き（Cone/地面デカール等）
        s.useAutoEndpoints = !fx.explicitEndpoints;
        YoRigine::EvaluateElementModules(asset, def, s);

        sub.tint            = s.colorTint;
        sub.beamRadiusScale = s.beamRadiusScale;
        sub.beamGlowScale   = s.beamGlowScale;
        sub.visible         = s.visible;

        if (sub.mesh) {
            sub.mesh->Drive(s);
            sub.mesh->Update(dt);
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

    for (auto& sub : fx.subs) {
        if (!sub.mesh || !sub.cbMapped || !sub.cbRes) continue;
        if (!sub.visible || sub.tint.w <= 0.001f) continue;

        // インスタンス色をモジュール評価結果（tint）に乗算する
        const Vector4 tint = {
            sub.tint.x * fx.userColor.x,
            sub.tint.y * fx.userColor.y,
            sub.tint.z * fx.userColor.z,
            sub.tint.w * fx.userColor.w
        };

        YoRigine::ProceduralMeshBase::CBFillArgs args{
            *sub.def, tint, fx.age, fx.burstProgress,
            sub.beamRadiusScale, sub.beamGlowScale
        };
        sub.mesh->FillCB(sub.cbMapped, args);

        const char* psoName = sub.mesh->GetPSOName();
        const auto& idx = pm->GetParameterIndices(psoName);
        const int blendOverride = sub.def ? sub.def->blendModeOverride : -1;
        cmdList->SetGraphicsRootSignature(pm->GetRootSignature(psoName));
        cmdList->SetPipelineState(pm->ResolveBlendPSO(psoName, blendOverride));
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gCamera"),    camAddr);
        cmdList->SetGraphicsRootConstantBufferView(idx.at("gMeshParam"), sub.cbRes->GetGPUVirtualAddress());
        sub.mesh->BindResources(cmdList, idx); // テクスチャ等(あれば)
        sub.mesh->Draw(cmdList);
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

void VfxMeshSpawner::SetTimeScale(uint32_t id, float timeScale)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->timeScale = (timeScale > 0.0001f) ? timeScale : 1.0f;
            return;
        }
    }
}

void VfxMeshSpawner::SetColor(uint32_t id, const Vector4& color)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            fx->userColor = color;
            return;
        }
    }
}

void VfxMeshSpawner::SetStyleIndices(uint32_t id, int a, int b, int c)
{
    for (ActiveEffect* fx : active_) {
        if (fx && fx->id == id) {
            for (auto& sub : fx->subs) {
                if (sub.mesh) sub.mesh->SetStyleIndices(a, b, c);
            }
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
        sub.mesh.reset();
    }
    subs.clear();
}
