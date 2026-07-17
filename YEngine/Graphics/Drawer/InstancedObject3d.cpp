#include "InstancedObject3d.h"

#include "DirectXCommon.h"
#include "DirectX/SrvManager.h"
#include "Systems/Camera/Camera.h"
#include "PipelineManager/YPipelineManager.h"
#include "LightManager/LightManager.h"
#include "Model.h"
#include "Material/OutlineSettings.h"
#include "WorldTransform/WorldTransform.h"
#include "Object3D/Object3d.h"
#include "Object3D/ObjectManager.h"

#include <cassert>

namespace {
	// バッチ初期容量 (壁を想定して 64 から始める)
	constexpr uint32_t kInitialCapacity = 64;
}

InstancedObject3d* InstancedObject3d::GetInstance()
{
	static InstancedObject3d instance;
	// 遅延初期化: 呼び出し側で Initialize を呼ばずに済むようにする
	if (!instance.dxCommon_) {
		instance.Initialize();
	}
	return &instance;
}

void InstancedObject3d::Initialize()
{
	if (dxCommon_) return; // 二重初期化防止
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

void InstancedObject3d::Finalize()
{
	for (auto& [model, batch] : batches_) {
		if (batch.mapped) {
			batch.gpuBuffer->Unmap(0, nullptr);
		}
		if (batch.shadowMapped) {
			batch.shadowGpuBuffer->Unmap(0, nullptr);
		}
	}
	batches_.clear();
	materialLighting_.reset();
}

void InstancedObject3d::Begin()
{
	for (auto& [model, batch] : batches_) {
		batch.cpuData.clear();
	}
	totalInstances_ = 0;
	frameCamera_ = nullptr;
}

void InstancedObject3d::Begin(Camera* camera)
{
	Begin();
	frameCamera_ = camera;
}

void InstancedObject3d::Submit(Model* model, const Instance& src)
{
	if (!model) return;

	InstanceData d{};
	d.World = src.world;
	// カメラ未設定 (影パス等 WVP を使わない用途) では WVP は World をそのまま入れておく
	d.WVP = frameCamera_ ? (src.world * frameCamera_->GetViewProjectionMatrix()) : src.world;
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

	AddInstance(model, d);
}

void InstancedObject3d::Submit(const ObjectManager::PlacedObject& placed)
{
	if (!placed.object || !placed.worldTransform) return;

	Submit(placed.object->GetModel(), Instance{
		placed.worldTransform->matWorld_,
		placed.color,
		MakeScaleMatrix({ placed.uvScale.x, placed.uvScale.y, 1.0f }),
		placed.uvStochastic,
		placed.outlineEnabled ? 1.0f : 0.0f,
	});
}

void InstancedObject3d::Submit(Object3d& object, const WorldTransform& transform)
{
	Submit(object.GetModel(), Instance{
		transform.matWorld_,
		object.GetColor(),
		object.GetUvTransform(),
		object.GetStochasticStrength(),
		object.IsOutlineEnabled() ? 1.0f : 0.0f,
	});
}

void InstancedObject3d::AddInstance(Model* model, const InstanceData& data)
{
	if (!model) return;
	auto& batch = batches_[model];
	batch.cpuData.push_back(data);
	++totalInstances_;
}

void InstancedObject3d::EnsureCapacity(Batch& batch, uint32_t needed)
{
	if (batch.capacity >= needed && batch.gpuBuffer) return;

	uint32_t newCap = batch.capacity == 0 ? kInitialCapacity : batch.capacity;
	while (newCap < needed) newCap *= 2;

	if (batch.mapped) {
		batch.gpuBuffer->Unmap(0, nullptr);
		batch.mapped = nullptr;
	}

	const size_t bufSize = sizeof(InstanceData) * newCap;
	batch.gpuBuffer = dxCommon_->CreateBufferResource(bufSize);
	batch.gpuBuffer->Map(0, nullptr, reinterpret_cast<void**>(&batch.mapped));
	batch.capacity = newCap;

	// SRV を作成 / 再作成
	if (batch.srvIndex == UINT32_MAX) {
		batch.srvIndex = srvManager_->Allocate();
	}
	srvManager_->CreateSRVforStructuredBuffer(
		batch.srvIndex, batch.gpuBuffer.Get(), newCap, sizeof(InstanceData));
	batch.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(batch.srvIndex);
}

void InstancedObject3d::EnsureShadowCapacity(Batch& batch, uint32_t needed)
{
	if (batch.shadowCapacity >= needed && batch.shadowGpuBuffer) return;

	uint32_t newCap = batch.shadowCapacity == 0 ? kInitialCapacity : batch.shadowCapacity;
	while (newCap < needed) newCap *= 2;

	if (batch.shadowMapped) {
		batch.shadowGpuBuffer->Unmap(0, nullptr);
		batch.shadowMapped = nullptr;
	}

	const size_t bufSize = sizeof(InstanceData) * newCap;
	batch.shadowGpuBuffer = dxCommon_->CreateBufferResource(bufSize);
	batch.shadowGpuBuffer->Map(0, nullptr, reinterpret_cast<void**>(&batch.shadowMapped));
	batch.shadowCapacity = newCap;

	if (batch.shadowSrvIndex == UINT32_MAX) {
		batch.shadowSrvIndex = srvManager_->Allocate();
	}
	srvManager_->CreateSRVforStructuredBuffer(
		batch.shadowSrvIndex, batch.shadowGpuBuffer.Get(), newCap, sizeof(InstanceData));
	batch.shadowSrvHandleGPU = srvManager_->GetGPUDescriptorHandle(batch.shadowSrvIndex);
}

void InstancedObject3d::DrawAll(Camera* camera)
{
	if (totalInstances_ == 0 || !camera) return;

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("ObjectInstanced");
	auto cmd = dxCommon_->GetCommandList().Get();

	// CBV/SRV/UAV ヒープを確実にバインド (シャドウパス→カラーパスの境界で heap が外れているケース対策)
	srvManager_->PreDraw();

	// === インバートハル輪郭線（本体より先に描き、内側は本体で上書きする） ===
	// 専用 PSO/RS へ切り替えてシェルを描く。直後に本体 PSO/RS を張り直すので復帰処理は不要。
	if (OutlineSettings::GetInstance()->IsEnabled()) {
		OutlineSettings::GetInstance()->UpdateCB();
		const auto& oidx = pm->GetParameterIndices("ObjectOutlineInstanced");

		cmd->SetPipelineState(pm->GetPipeLineStateObject("ObjectOutlineInstanced"));
		cmd->SetGraphicsRootSignature(pm->GetRootSignature("ObjectOutlineInstanced"));
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cmd->SetGraphicsRootConstantBufferView(
			oidx.at("gCamera"), camera->GetCameraResource()->GetGPUVirtualAddress());
		cmd->SetGraphicsRootConstantBufferView(
			oidx.at("gOutline"), OutlineSettings::GetInstance()->GetResource()->GetGPUVirtualAddress());

		for (auto& [model, batch] : batches_) {
			const uint32_t count = static_cast<uint32_t>(batch.cpuData.size());
			if (count == 0) continue;

			EnsureCapacity(batch, count);
			std::memcpy(batch.mapped, batch.cpuData.data(), sizeof(InstanceData) * count);

			model->DrawOutlineInstanced(count, batch.srvHandleGPU);
		}
	}

	// PSO + RS + topology
	cmd->SetPipelineState(pm->GetPipeLineStateObject("ObjectInstanced"));
	cmd->SetGraphicsRootSignature(pm->GetRootSignature("ObjectInstanced"));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ライトCB (Directional / Point / Spot)
	YoRigine::LightManager::GetInstance()->SetCommandList(
		indices.at("gDirectionalLight"),
		indices.at("gPointLights"),
		indices.at("gSpotLights"));

	// Light VP 配列 (カラーパス PS のカスケード選択用)
	cmd->SetGraphicsRootConstantBufferView(
		indices.at("gCascadeShadow"),
		YoRigine::LightManager::GetInstance()->GetCascadeResource()->GetGPUVirtualAddress());

	// Camera
	cmd->SetGraphicsRootConstantBufferView(
		indices.at("gCamera"),
		camera->GetCameraResource()->GetGPUVirtualAddress());

	// MaterialLight (共有CB)。RecordDrawCommands 内でグローバルなトゥーン設定が反映される。
	materialLighting_->RecordDrawCommands(cmd, indices.at("gMaterialLight"));

	// 各バッチを描画
	for (auto& [model, batch] : batches_) {
		const uint32_t count = static_cast<uint32_t>(batch.cpuData.size());
		if (count == 0) continue;

		EnsureCapacity(batch, count);
		std::memcpy(batch.mapped, batch.cpuData.data(), sizeof(InstanceData) * count);

		model->DrawInstanced(count, batch.srvHandleGPU);
	}
}

void InstancedObject3d::DrawShadow()
{
	if (totalInstances_ == 0) return;

	auto pm = YPipelineManager::GetInstance();
	auto cmd = dxCommon_->GetCommandList().Get();

	// シャドウパスでは旧来 descriptor heap がバインドされていない場合があるため必ず PreDraw
	srvManager_->PreDraw();

	cmd->SetPipelineState(pm->GetPipeLineStateObject("ShadowMapInstanced"));
	cmd->SetGraphicsRootSignature(pm->GetRootSignature("ShadowMapInstanced"));
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const auto& indices = pm->GetParameterIndices("ShadowMapInstanced");
	cmd->SetGraphicsRootConstantBufferView(
		indices.at("gLight"),
		YoRigine::LightManager::GetInstance()->GetShadowResource()->GetGPUVirtualAddress());

	// 影パスは専用バッファ (shadowGpuBuffer) へ書き込む。
	// カラーパスと共有すると、後から記録されるカラーパス (frustum culling 付き) の
	// memcpy が GPU 実行前に中身を上書きし、影の内容が「カリング後の別の集合」に
	// すり替わって広範囲の影が出たり消えたりチラつく。
	for (auto& [model, batch] : batches_) {
		const uint32_t count = static_cast<uint32_t>(batch.cpuData.size());
		if (count == 0) continue;

		EnsureShadowCapacity(batch, count);
		std::memcpy(batch.shadowMapped, batch.cpuData.data(), sizeof(InstanceData) * count);

		model->DrawShadowInstanced(count, batch.shadowSrvHandleGPU);
	}

	// ── 後始末: 非インスタンス影パイプライン("ShadowMap")を復元する ──
	// Object3d::DrawShadow は ShadowDrawPreference が張った "ShadowMap" PSO/RS に
	// 依存して自分では張らない。本関数が "ShadowMapInstanced" に切り替えたまま戻ると、
	// この後に呼ばれた Object3d::DrawShadow がインスタンス用 VS + 残った SRV で
	// ゴミ行列のジオメトリをシャドウマップへ焼く（広大な影チラつきの原因）。
	// → 呼び出し順に依存しないよう、ここで必ず復元しておく。
	{
		const auto& sidx = pm->GetParameterIndices("ShadowMap");
		cmd->SetPipelineState(pm->GetPipeLineStateObject("ShadowMap"));
		cmd->SetGraphicsRootSignature(pm->GetRootSignature("ShadowMap"));
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->SetGraphicsRootConstantBufferView(
			sidx.at("gLight"),
			YoRigine::LightManager::GetInstance()->GetShadowResource()->GetGPUVirtualAddress());
	}
}
