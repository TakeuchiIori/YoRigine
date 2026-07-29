#include "InstancedObject3d.h"

#include "DirectX/SrvManager.h"
#include "DirectXCommon.h"
#include "LightManager/LightManager.h"
#include "Material/OutlineSettings.h"
#include "Model.h"
#include "Object3D/Object3d.h"
#include "Object3D/ObjectManager.h"
#include "PipelineManager/YPipelineManager.h"
#include "Systems/Camera/Camera.h"
#include "WorldTransform/WorldTransform.h"

#include <cassert>

namespace {
// バッチ初期容量 (壁を想定して 64 から始める)
constexpr uint32_t kInitialCapacity = 64;
} // namespace

InstancedObject3d *InstancedObject3d::GetInstance() {
  static InstancedObject3d instance;
  // 遅延初期化: 呼び出し側で Initialize を呼ばずに済むようにする
  if (!instance.dxCommon_) {
    instance.Initialize();
  }
  return &instance;
}

void InstancedObject3d::Initialize() {
  if (dxCommon_)
    return; // 二重初期化防止
  dxCommon_ = YoRigine::DirectXCommon::GetInstance();
  srvManager_ = YoRigine::SrvManager::GetInstance();

  // 共有 MaterialLight（lighting有効, specular/env無効, shininess=8）。
  // MaterialLighting 経由なのでトゥーン等のグローバル設定も自動で反映される。
  materialLighting_ = std::make_unique<MaterialLighting>();
  materialLighting_->Initialize();
  materialLighting_->SetEnableLighting(true);
  materialLighting_->SetEnableSpecular(false);
  materialLighting_->SetEnableEnvironment(false);
  materialLighting_->SetIsHalfVector(false);
  materialLighting_->SetShininess(8.0f);
  materialLighting_->SetEnvironmentCoefficient(0.0f);
}

void InstancedObject3d::Finalize() {
  for (auto &[key, batch] : batches_) {
    for (auto &slot : batch.colorSlots) {
      if (slot.mapped) {
        slot.gpuBuffer->Unmap(0, nullptr);
      }
    }
    for (auto &slot : batch.shadowSlots) {
      if (slot.mapped) {
        slot.gpuBuffer->Unmap(0, nullptr);
      }
    }
  }
  batches_.clear();
  materialLighting_.reset();
}

void InstancedObject3d::Begin() {
  for (auto &[key, batch] : batches_) {
    batch.cpuData.clear();
  }
  totalInstances_ = 0;
  frameCamera_ = nullptr;
}

void InstancedObject3d::Begin(YoRigine::Camera *camera) {
  Begin();
  frameCamera_ = camera;
}

void InstancedObject3d::Submit(YoRigine::Model *model, const Instance &src,
                               const std::string &overrideTexturePath,
                               MaterialOverrideSet *materialOverrides) {
  if (!model)
    return;

  InstanceData d{};
  d.World = src.world;
  // カメラ未設定 (影パス等 WVP を使わない用途) では WVP は World
  // をそのまま入れておく
  d.WVP = frameCamera_ ? (src.world * frameCamera_->GetViewProjectionMatrix())
                       : src.world;
  {
    const Matrix4x4 inv = Inverse(src.world);
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        d.WorldInverseTranspose.m[r][c] = inv.m[c][r];
  }
  d.color = src.color;
  d.uvTransform = src.uvTransform;
  d.stochasticStrength = src.stochasticStrength;
  d.outlineMask = src.outlineMask;
  d.ditherFade = src.ditherFade;

  AddInstance(model, d, overrideTexturePath, materialOverrides);
}

void InstancedObject3d::Submit(const ObjectManager::PlacedObject &placed) {
  if (!placed.object || !placed.worldTransform)
    return;

  Submit(placed.object->GetModel(),
         Instance{
             placed.worldTransform->matWorld_,
             placed.color,
             MakeScaleMatrix({placed.uvScale.x, placed.uvScale.y, 1.0f}),
             placed.uvStochastic,
             placed.outlineEnabled ? 1.0f : 0.0f,
         },
         placed.object->GetOverrideTexturePath(),
         placed.object->GetActiveMaterialOverrides());
}

void InstancedObject3d::Submit(YoRigine::Object3d &object,
                               const YoRigine::WorldTransform &transform) {
  YoRigine::Model *model = object.GetModel();
  if (!model)
    return;
  const Matrix4x4 visualWorld =
      model->GetHasBones()
          ? transform.matWorld_
          : (transform.matWorld_ * model->GetRootNode().GetLocalMatrix());
  Submit(model,
         Instance{
             visualWorld,
             object.GetColor(),
             object.GetUvTransform(),
             object.GetStochasticStrength(),
             object.IsOutlineEnabled() ? 1.0f : 0.0f,
         },
         object.GetOverrideTexturePath(), object.GetActiveMaterialOverrides());
}

void InstancedObject3d::AddInstance(YoRigine::Model *model,
                                    const InstanceData &data,
                                    const std::string &overrideTexturePath,
                                    MaterialOverrideSet *materialOverrides) {
  if (!model)
    return;
  // 実際には何も上書きしていないセットでバッチを分けない (インスタンス数を保つ)
  if (materialOverrides && !materialOverrides->HasAnyOverride()) {
    materialOverrides = nullptr;
  }
  if (materialOverrides) {
    // GPU へ書き込むのは dirty のときだけなので毎フレーム呼んでよい
    materialOverrides->Apply(*model);
  }
  auto &batch =
      batches_[BatchKey{model, overrideTexturePath, materialOverrides}];
  batch.cpuData.push_back(data);
  ++totalInstances_;
}

void InstancedObject3d::SubmitDitherFade(
    const ObjectManager::PlacedObject &placed) {
  if (!placed.object || !placed.worldTransform)
    return;
  Submit(placed.object->GetModel(),
         Instance{
             placed.worldTransform->matWorld_,
             placed.color,
             MakeScaleMatrix({placed.uvScale.x, placed.uvScale.y, 1.0f}),
             placed.uvStochastic,
             0.0f, // フェード途中に不透明な輪郭だけが残るのを防ぐ
             1.0f,
         },
         placed.object->GetOverrideTexturePath(),
         placed.object->GetActiveMaterialOverrides());
}

void InstancedObject3d::EnsureCapacity(Batch::BufferSlot &slot,
                                       uint32_t needed) {
  if (slot.capacity >= needed && slot.gpuBuffer)
    return;

  uint32_t newCap = slot.capacity == 0 ? kInitialCapacity : slot.capacity;
  while (newCap < needed)
    newCap *= 2;

  if (slot.mapped) {
    slot.gpuBuffer->Unmap(0, nullptr);
    slot.mapped = nullptr;
  }

  const size_t bufSize = sizeof(InstanceData) * newCap;
  slot.gpuBuffer = dxCommon_->CreateBufferResource(bufSize);
  slot.gpuBuffer->Map(0, nullptr, reinterpret_cast<void **>(&slot.mapped));
  slot.capacity = newCap;

  // SRV を作成 / 再作成
  if (slot.srvIndex == UINT32_MAX) {
    slot.srvIndex = srvManager_->Allocate();
  }
  srvManager_->CreateSRVforStructuredBuffer(slot.srvIndex, slot.gpuBuffer.Get(),
                                            newCap, sizeof(InstanceData));
  slot.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(slot.srvIndex);
}

void InstancedObject3d::EnsureShadowCapacity(Batch::BufferSlot &slot,
                                             uint32_t needed) {
  if (slot.capacity >= needed && slot.gpuBuffer)
    return;

  uint32_t newCap = slot.capacity == 0 ? kInitialCapacity : slot.capacity;
  while (newCap < needed)
    newCap *= 2;

  if (slot.mapped) {
    slot.gpuBuffer->Unmap(0, nullptr);
    slot.mapped = nullptr;
  }

  const size_t bufSize = sizeof(InstanceData) * newCap;
  slot.gpuBuffer = dxCommon_->CreateBufferResource(bufSize);
  slot.gpuBuffer->Map(0, nullptr, reinterpret_cast<void **>(&slot.mapped));
  slot.capacity = newCap;

  if (slot.srvIndex == UINT32_MAX) {
    slot.srvIndex = srvManager_->Allocate();
  }
  srvManager_->CreateSRVforStructuredBuffer(slot.srvIndex, slot.gpuBuffer.Get(),
                                            newCap, sizeof(InstanceData));
  slot.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(slot.srvIndex);
}

InstancedObject3d::Batch::BufferSlot &
InstancedObject3d::AcquireColorSlot(Batch &batch, uint32_t needed) {
  const uint32_t frameIndex = dxCommon_->GetCurrentBackBufferIndex();
  if (batch.colorFrameIndex != frameIndex) {
    batch.colorFrameIndex = frameIndex;
    batch.nextColorSlot = 0;
  }
  const size_t slotIndex = batch.nextColorSlot++;
  if (batch.colorSlots.size() <= slotIndex) {
    batch.colorSlots.emplace_back();
  }
  auto &slot = batch.colorSlots[slotIndex];
  EnsureCapacity(slot, needed);
  return slot;
}

InstancedObject3d::Batch::BufferSlot &
InstancedObject3d::AcquireShadowSlot(Batch &batch, uint32_t needed) {
  const uint32_t frameIndex = dxCommon_->GetCurrentBackBufferIndex();
  if (batch.shadowFrameIndex != frameIndex) {
    batch.shadowFrameIndex = frameIndex;
    batch.nextShadowSlot = 0;
  }
  const size_t slotIndex = batch.nextShadowSlot++;
  if (batch.shadowSlots.size() <= slotIndex) {
    batch.shadowSlots.emplace_back();
  }
  auto &slot = batch.shadowSlots[slotIndex];
  EnsureShadowCapacity(slot, needed);
  return slot;
}

void InstancedObject3d::DrawAll(YoRigine::Camera *camera) {
  if (totalInstances_ == 0 || !camera)
    return;

  auto pm = YPipelineManager::GetInstance();
  const auto &indices = pm->GetParameterIndices("ObjectInstanced");
  auto cmd = dxCommon_->GetCommandList().Get();

  // CBV/SRV/UAV ヒープを確実にバインド (シャドウパス→カラーパスの境界で heap
  // が外れているケース対策)
  srvManager_->PreDraw();

  // === インバートハル輪郭線（本体より先に描き、内側は本体で上書きする） ===
  // 専用 PSO/RS へ切り替えてシェルを描く。直後に本体 PSO/RS
  // を張り直すので復帰処理は不要。
  if (OutlineSettings::GetInstance()->IsEnabled()) {
    OutlineSettings::GetInstance()->UpdateCB();
    const auto &oidx = pm->GetParameterIndices("ObjectOutlineInstanced");

    cmd->SetPipelineState(pm->GetPipeLineStateObject("ObjectOutlineInstanced"));
    cmd->SetGraphicsRootSignature(
        pm->GetRootSignature("ObjectOutlineInstanced"));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmd->SetGraphicsRootConstantBufferView(
        oidx.at("gCamera"),
        camera->GetCameraResource()->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(
        oidx.at("gOutline"),
        OutlineSettings::GetInstance()->GetResource()->GetGPUVirtualAddress());

    for (auto &[key, batch] : batches_) {
      const uint32_t count = static_cast<uint32_t>(batch.cpuData.size());
      if (count == 0)
        continue;

      auto &slot = AcquireColorSlot(batch, count);
      std::memcpy(slot.mapped, batch.cpuData.data(),
                  sizeof(InstanceData) * count);

      key.model->DrawOutlineInstanced(count, slot.srvHandleGPU);
    }
  }

  // PSO + RS + topology
  cmd->SetPipelineState(pm->GetPipeLineStateObject("ObjectInstanced"));
  cmd->SetGraphicsRootSignature(pm->GetRootSignature("ObjectInstanced"));
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // ライトCB (Directional / Point / Spot)
  YoRigine::LightManager::GetInstance()->SetCommandList(
      indices.at("gDirectionalLight"), indices.at("gPointLights"),
      indices.at("gSpotLights"));

  // Light VP 配列 (カラーパス PS のカスケード選択用)
  cmd->SetGraphicsRootConstantBufferView(indices.at("gCascadeShadow"),
                                         YoRigine::LightManager::GetInstance()
                                             ->GetCascadeResource()
                                             ->GetGPUVirtualAddress());

  // Camera
  cmd->SetGraphicsRootConstantBufferView(
      indices.at("gCamera"),
      camera->GetCameraResource()->GetGPUVirtualAddress());

  // MaterialLight (共有CB)。RecordDrawCommands
  // 内でグローバルなトゥーン設定が反映される。
  materialLighting_->RecordDrawCommands(cmd, indices.at("gMaterialLight"));

  // 各バッチを描画 (テクスチャ上書きがあればバッチ単位で差し替える)
  for (auto &[key, batch] : batches_) {
    const uint32_t count = static_cast<uint32_t>(batch.cpuData.size());
    if (count == 0)
      continue;

    auto &slot = AcquireColorSlot(batch, count);
    std::memcpy(slot.mapped, batch.cpuData.data(),
                sizeof(InstanceData) * count);

    key.model->DrawInstanced(count, slot.srvHandleGPU, key.overrideTexturePath,
                             key.materialOverrides);
  }
}

void InstancedObject3d::DrawShadow() {
  if (totalInstances_ == 0)
    return;

  auto pm = YPipelineManager::GetInstance();
  auto cmd = dxCommon_->GetCommandList().Get();

  // シャドウパスでは旧来 descriptor heap
  // がバインドされていない場合があるため必ず PreDraw
  srvManager_->PreDraw();

  cmd->SetPipelineState(pm->GetPipeLineStateObject("ShadowMapInstanced"));
  cmd->SetGraphicsRootSignature(pm->GetRootSignature("ShadowMapInstanced"));
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  const auto &indices = pm->GetParameterIndices("ShadowMapInstanced");
  cmd->SetGraphicsRootConstantBufferView(indices.at("gLight"),
                                         YoRigine::LightManager::GetInstance()
                                             ->GetShadowResource()
                                             ->GetGPUVirtualAddress());

  // 影パスも DrawShadow 呼び出しごとに別スロットへ書く。
  // 同一フレーム内で後続のインスタンス描画が同じバッファを上書きすると、
  // GPU 実行時に先に記録した描画コマンドの行列がすり替わるため。
  for (auto &[key, batch] : batches_) {
    const uint32_t count = static_cast<uint32_t>(batch.cpuData.size());
    if (count == 0)
      continue;

    auto &slot = AcquireShadowSlot(batch, count);
    std::memcpy(slot.mapped, batch.cpuData.data(),
                sizeof(InstanceData) * count);

    key.model->DrawShadowInstanced(count, slot.srvHandleGPU);
  }

  // ── 後始末: 非インスタンス影パイプライン("ShadowMap")を復元する ──
  // Object3d::DrawShadow は ShadowDrawPreference が張った "ShadowMap" PSO/RS に
  // 依存して自分では張らない。本関数が "ShadowMapInstanced"
  // に切り替えたまま戻ると、 この後に呼ばれた Object3d::DrawShadow
  // がインスタンス用 VS + 残った SRV で
  // ゴミ行列のジオメトリをシャドウマップへ焼く（広大な影チラつきの原因）。
  // → 呼び出し順に依存しないよう、ここで必ず復元しておく。
  {
    const auto &sidx = pm->GetParameterIndices("ShadowMap");
    cmd->SetPipelineState(pm->GetPipeLineStateObject("ShadowMap"));
    cmd->SetGraphicsRootSignature(pm->GetRootSignature("ShadowMap"));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootConstantBufferView(sidx.at("gLight"),
                                           YoRigine::LightManager::GetInstance()
                                               ->GetShadowResource()
                                               ->GetGPUVirtualAddress());
  }
}
