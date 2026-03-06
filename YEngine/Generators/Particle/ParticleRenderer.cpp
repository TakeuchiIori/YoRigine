#include "ParticleRenderer.h"

// Engine
#include "PipelineManager/YPipelineManager.h"
#include "ParticleSystem.h"
#include "Mesh/Mesh.h"
#include "LightManager/LightManager.h"

//=================================================================
// 初期化
//=================================================================

/// <summary>
/// パーティクルレンダラーの初期化（バッファ生成・パイプライン取得）
/// </summary>
void ParticleRenderer::Initialize(SrvManager* srvManager) {
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	srvManager_ = srvManager;

	// LightManagerを取得
	lightManager_ = YoRigine::LightManager::GetInstance();

	// マテリアル用CB
	CreateMaterialResource();

	// パーティクル用ルートシグネチャ＆PSO取得
	rootSignature_ = YPipelineManager::GetInstance()->GetRootSignature("Particle");
	graphicsPipelineState_ = YPipelineManager::GetInstance()->GetPipeLineStateObject("Particle");

	// トレイル用も同じパイプラインを利用
	trailRootSignature_ = YPipelineManager::GetInstance()->GetRootSignature("Particle");
	trailPipelineState_ = YPipelineManager::GetInstance()->GetPipeLineStateObject("Particle");
}

/// <summary>
/// 使用した GPU リソースの解放
/// </summary>
void ParticleRenderer::Finalize() {
	if (materialResource_) {
		materialResource_->Unmap(0, nullptr);
		materialResource_.Reset();
	}
	materialData_ = nullptr;
}

//=================================================================
// マテリアル・ライトバッファ生成
//=================================================================

/// <summary>
/// マテリアル用定数バッファの作成
/// </summary>
void ParticleRenderer::CreateMaterialResource() {
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// デフォルト値
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->color = { 1,1,1,1 };
	materialData_->enableLighting = 0;  // デフォルトはライティング無効
}

//=================================================================
// メインパーティクル描画
//=================================================================

/// <summary>
/// パーティクルシステム1つ分を描画
/// </summary>
void ParticleRenderer::RenderSystem(ParticleSystem& system) {
	if (system.GetParticleCount() == 0) return;

	auto mesh = system.GetMesh();
	if (!mesh) return;

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Particle");

	// GPUインスタンシングデータ更新
	UpdateInstanceData(system);

	// PSO + RootSig + Topology セット
	SetPipeline(system.GetBlendMode());

	// マテリアル/ライトのCBuffer更新
	UpdateMaterialData(system);

	// LightManagerのライト情報をセット
	if (lightManager_) {
		lightManager_->SetCommandListDirectionalOnly(indices.at("gDirectionalLight"));
	}

	// テクスチャ（SRV）設定
	SetupTexture(system.GetTexture(), system.GetTextureIndexSRV());

	// インスタンス描画
	DrawInstances(mesh, system.GetInstanceCount(), system.GetSRVIndex());
}

//=================================================================
// トレイル描画
//=================================================================

/// <summary>
/// パーティクルのトレイル描画
/// </summary>
void ParticleRenderer::RenderTrails(ParticleSystem& system) {
	(void)system;
	//if (!system.GetSettings().GetTrailEnabled() ||
	//	system.GetTrailInstanceCount() == 0) return;

	//auto commandList = dxCommon_->GetCommandList();

	//// トレイル専用 PSO / RootSig
	//commandList->SetPipelineState(trailPipelineState_.Get());
	//commandList->SetGraphicsRootSignature(trailRootSignature_.Get());
	//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//// マテリアルCB設定
	//commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	//// LightManagerのライト情報をセット
	//if (lightManager_) {
	//	lightManager_->SetCommandList();
	//}

	//// VB / IB 設定
	//auto vb = system.GetTrailVertexBuffer();
	//auto ib = system.GetTrailIndexBuffer();

	//if (vb && ib && system.GetTrailVertexCount() > 0) {
	//	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	//	vbv.BufferLocation = vb->GetGPUVirtualAddress();
	//	vbv.SizeInBytes = static_cast<UINT>(system.GetTrailVertexCount() * sizeof(ParticleSystem::TrailVertex));
	//	vbv.StrideInBytes = sizeof(ParticleSystem::TrailVertex);

	//	D3D12_INDEX_BUFFER_VIEW ibv = {};
	//	ibv.BufferLocation = ib->GetGPUVirtualAddress();
	//	ibv.SizeInBytes = static_cast<UINT>(system.GetTrailIndexCount() * sizeof(uint32_t));
	//	ibv.Format = DXGI_FORMAT_R32_UINT;

	//	commandList->IASetVertexBuffers(0, 1, &vbv);
	//	commandList->IASetIndexBuffer(&ibv);

	//	// トレイルのテクスチャ
	//	SetupTexture(system.GetTexture(), system.GetTextureIndexSRV());

	//	// 描画
	//	commandList->DrawIndexedInstanced(
	//		(UINT)system.GetTrailIndexCount(),
	//		1,
	//		0, 0, 0
	//	);
	//}
}

//=================================================================
// PSO / RootSig / トポロジ設定
//=================================================================

/// <summary>
/// ブレンドモードに応じてPSOセット
/// </summary>
void ParticleRenderer::SetPipeline(BlendMode blendMode) {
	auto commandList = dxCommon_->GetCommandList();

	// ブレンドモード対応のPSO取得
	auto pso = YPipelineManager::GetInstance()->GetBlendModePSO("Particle",blendMode);
	commandList->SetPipelineState(pso);
	// Root Signature セット
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Particle");

	// 三角形リスト
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// マテリアルCB
	commandList->SetGraphicsRootConstantBufferView(indices.at("gMaterial"), materialResource_->GetGPUVirtualAddress());
}

//=================================================================
// 各種データ更新
//=================================================================

/// <summary>
/// マテリアル情報をGPUバッファへ反映
/// </summary>
void ParticleRenderer::UpdateMaterialData(const ParticleSystem& system) {
	if (!materialData_) return;

	auto materialInfo = system.GetMaterialInfo();
	materialData_->uvTransform = materialInfo.uvTransform;
	materialData_->color = materialInfo.color;
	materialData_->enableLighting = system.GetSettings().GetEnableLighting() ? 1 : 0;  // ライティング設定を反映
}

/// <summary>
/// GPU用インスタンシングデータ作成
/// </summary>
void ParticleRenderer::UpdateInstanceData(ParticleSystem& system) {
	system.PrepareInstancingData(camera_);
}

//=================================================================
// テクスチャ設定
//=================================================================

/// <summary>
/// パーティクル用テクスチャ（SRV）をスロットにセット
/// </summary>
void ParticleRenderer::SetupTexture(const std::string& textureFilePath, uint32_t textureIndexSRV) {
	if (textureFilePath.empty()) return;
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Particle");
	srvManager_->SetGraphicsRootDescriptorTable(indices.at("gTexture"), textureIndexSRV);
}

//=================================================================
// 描画処理
//=================================================================

/// <summary>
/// メッシュをインスタンシング描画
/// </summary>
void ParticleRenderer::DrawInstances(const std::shared_ptr<Mesh>& mesh, uint32_t instanceCount, uint32_t srvIndex) {
	auto commandList = dxCommon_->GetCommandList();
	auto& meshRes = mesh->GetMeshResource();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Particle");

	// メッシュ描画用VB/IB設定
	commandList->IASetVertexBuffers(0, 1, &meshRes.vertexBufferView);
	commandList->IASetIndexBuffer(&meshRes.indexBufferView);

	// インスタンシングデータのSRV設定
	srvManager_->SetGraphicsRootDescriptorTable(indices.at("gParticle"), srvIndex);

	// 描画コール
	commandList->DrawIndexedInstanced(
		mesh->GetIndexCount(),
		instanceCount,
		0, 0, 0
	);
}