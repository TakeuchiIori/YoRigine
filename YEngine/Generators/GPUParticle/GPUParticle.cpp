#include "GPUParticle.h"
#include <cassert>
#include <SrvManager.h>
#include <Loaders/Texture/TextureManager.h>
#include "Systems/GameTime/GameTime.h"


#ifdef USE_IMGUI
#include <imgui.h>
#endif

/// <summary>
/// GPU パーティクルの初期化処理（各種バッファ生成）
/// </summary>
void GPUParticle::Initialize(const std::string& filepath, Camera* camera)
{
	textureFilePath_ = filepath;
	camera_ = camera;
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	pipelineManager_ = YPipelineManager::GetInstance();
	computeShaderManager_ = ComputeShaderManager::GetInstance();

	mesh_ = std::make_shared<Mesh>();
	materialColor_ = std::make_unique<MaterialColor>();    materialColor_->Initialize();
	materialUV_ = std::make_unique<MaterialUV>();       materialUV_->Initialize();


	CreateVertexResource();
	CreatePerViewResource();

	CreateUAV();
	CreateGPUParticleResource();
	CreateTexture();

	// Readback for ActiveCount
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = sizeof(uint32_t);
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&activeCountReadback_)
	);

	// 初期値 0 にしておく
	cachedActiveCount_ = 0;


	// 初回初期化用コンピュート
	DispatchInit();
}

/// <summary>
/// 毎フレーム更新
/// </summary>
void GPUParticle::Update(ID3D12Resource* resource, ID3D12Resource* paramsResource)
{
	UpdatePerView();
	DispatchUpdate(resource,paramsResource);

	auto commandList = dxCommon_->GetCommandList();

	// 今フレームぶんの ActiveCount を Readback にコピー
	dxCommon_->TransitionBarrier(
		activeCountResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);

	commandList->CopyResource(activeCountReadback_.Get(), activeCountResource_.Get());

	dxCommon_->TransitionBarrier(
		activeCountResource_.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	// 前フレームぶんを読む
	uint32_t* pData = nullptr;
	if (SUCCEEDED(activeCountReadback_->Map(0, nullptr, reinterpret_cast<void**>(&pData)))) {
		cachedActiveCount_ = *pData;
		activeCountReadback_->Unmap(0, nullptr);
	}

	// cachedStats_ 側に反映
	cachedStats_.maxParticles = kMaxParticles;
	cachedStats_.activeCount = cachedActiveCount_;
	cachedStats_.freeCount = kMaxParticles - cachedActiveCount_;
	cachedStats_.usagePercent =
		(float)cachedStats_.activeCount / (float)kMaxParticles * 100.0f;
	cachedStats_.isValid = true;
}


/// <summary>
/// GPU パーティクル描画
/// </summary>
void GPUParticle::Draw()
{
	auto commandList = dxCommon_->GetCommandList();

	//-----------------------------------------
	// パイプライン設定
	//-----------------------------------------
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("GPUParticleInit");

	auto pso = YPipelineManager::GetInstance()->GetBlendModePSO("GPUParticleInit",blendMode_);
	commandList->SetPipelineState(pso);
	commandList->SetGraphicsRootSignature(pipelineManager_->GetRootSignature("GPUParticleInit"));

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//-----------------------------------------
	// 頂点・インデックス設定
	//-----------------------------------------
	auto meshResource = mesh_->GetMeshResource();
	commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBufferView);
	commandList->IASetIndexBuffer(&meshResource.indexBufferView);

	//-----------------------------------------
	// Root パラメータ
	//-----------------------------------------
	
	commandList->SetGraphicsRootConstantBufferView(indices.at("g_PerView"), perViewResource_->GetGPUVirtualAddress());
	// マテリアル
	materialUV_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialUV"));
	materialColor_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialColor"));
	commandList->SetGraphicsRootDescriptorTable(indices.at("g_Particles"), particleSrvHandleGPU_);
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), textureIndexSRV_);

	//-----------------------------------------
	// 描画
	//-----------------------------------------
	commandList->DrawIndexedInstanced(mesh_->GetIndexCount(), kMaxParticles, 0, 0, 0);
}

void GPUParticle::Reset()
{
	DispatchInit();
	// 初期値 0 にしておく
	cachedStats_.maxParticles = kMaxParticles;
	cachedStats_.activeCount = 0;
	cachedStats_.freeCount = kMaxParticles;
	cachedStats_.usagePercent = 0.0f;
	cachedStats_.freeListIndex = 0;
	cachedStats_.isValid = true;
}


/// <summary>
/// UAV リソース作成（パーティクル・フリーリスト）
/// </summary>
void GPUParticle::CreateUAV()
{
	//-----------------------------------------
	// Particle UAV
	//-----------------------------------------
	uavIndex_ = SrvManager::GetInstance()->Allocate();

	particleResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(ParticleCSForGPU) * kMaxParticles);

	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		uavIndex_,
		particleResource_.Get(),
		kMaxParticles,
		sizeof(ParticleCSForGPU)
	);

	particleUavHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(uavIndex_);

	//-----------------------------------------
	// FreeListIndex UAV
	//-----------------------------------------
	freeListIndexUavIndex_ = SrvManager::GetInstance()->Allocate();

	freeListIndexResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(int32_t));

	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		freeListIndexUavIndex_,
		freeListIndexResource_.Get(),
		1,
		sizeof(int32_t)
	);

	freeListIndexUavHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(freeListIndexUavIndex_);

	//-----------------------------------------
	// FreeList UAV
	//-----------------------------------------
	freeListUavIndex_ = SrvManager::GetInstance()->Allocate();

	freeListResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(uint32_t) * kMaxParticles);

	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		freeListUavIndex_,
		freeListResource_.Get(),
		kMaxParticles,
		sizeof(uint32_t)
	);

	freeListUavHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(freeListUavIndex_);

	//-----------------------------------------
	// ActiveCount UAV
	//-----------------------------------------
	activeCountUavIndex_ = SrvManager::GetInstance()->Allocate();

	activeCountResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(uint32_t));

	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		activeCountUavIndex_,
		activeCountResource_.Get(),
		1,
		sizeof(uint32_t)
	);

	activeCountUavHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(activeCountUavIndex_);

}

/// <summary>
/// GPU パーティクルの SRV 作成（パーティクルを読み込む）
/// </summary>
void GPUParticle::CreateGPUParticleResource()
{
	srvIndex_ = SrvManager::GetInstance()->Allocate();

	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
		srvIndex_,
		particleResource_.Get(),
		kMaxParticles,
		sizeof(ParticleCSForGPU)
	);

	particleSrvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
}

/// <summary>
/// PerView用 CB 作成
/// </summary>
void GPUParticle::CreatePerViewResource()
{
	perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerViewForGPU));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

	perViewData_->viewProjection = MakeIdentity4x4();
	perViewData_->billboardMatrix = MakeIdentity4x4();
	perViewData_->isBillboard = billboard_ ? 1u : 0u;
}

/// <summary>
/// パーティクル描画用の頂点（板ポリ）
/// </summary>
void GPUParticle::CreateVertexResource()
{
	mesh_ = MeshPrimitive::CreatePlane(1.0f, 1.0f);
}

/// <summary>
/// 初期化用 ComputeShader の実行
/// </summary>
void GPUParticle::DispatchInit()
{
	auto commandList = dxCommon_->GetCommandList();

	//-----------------------------------------
	// UAV バリア（書き込み許可）
	//-----------------------------------------
	dxCommon_->TransitionBarrier(
		particleResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		freeListIndexResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		freeListResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	// ActiveCount は UAV としてしか使わないので COMMON → UAV にしてそのまま
	dxCommon_->TransitionBarrier(
		activeCountResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	//-----------------------------------------
	// CS Pipeline
	//-----------------------------------------
	commandList->SetComputeRootSignature(computeShaderManager_->GetRootSignature("ParticleInitCS"));

	commandList->SetPipelineState(computeShaderManager_->GetComputePipelineState("ParticleInitCS"));

	ID3D12DescriptorHeap* heaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	commandList->SetComputeRootDescriptorTable(0, particleUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(1, freeListIndexUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(2, freeListUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(3, activeCountUavHandleGPU_);

	uint32_t requiredGroups = GPUParticle::GetRequiredThreadGroups();
	commandList->Dispatch(requiredGroups, 1, 1);

	//-----------------------------------------
	// 状態戻し
	//-----------------------------------------
	dxCommon_->TransitionBarrier(
		particleResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);

	dxCommon_->TransitionBarrier(
		freeListIndexResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		freeListResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
}

/// <summary>
/// Update 用 ComputeShader 実行（Emit されたパーティクルを更新）
/// </summary>
void GPUParticle::DispatchUpdate(ID3D12Resource* resource, ID3D12Resource* paramsResource)
{
	auto commandList = dxCommon_->GetCommandList();

	//-----------------------------------------
	// UAV 書き込み準備
	//-----------------------------------------
	dxCommon_->TransitionBarrier(
		particleResource_.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		freeListIndexResource_.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		freeListResource_.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);


	//-----------------------------------------
	// CS Pipeline
	//-----------------------------------------
	commandList->SetComputeRootSignature(computeShaderManager_->GetRootSignature("ParticleUpdateCS"));
	commandList->SetPipelineState(computeShaderManager_->GetComputePipelineState("ParticleUpdateCS"));

	ID3D12DescriptorHeap* heaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	commandList->SetComputeRootDescriptorTable(0, particleUavHandleGPU_);
	commandList->SetComputeRootConstantBufferView(1, resource->GetGPUVirtualAddress());
	commandList->SetComputeRootConstantBufferView(2, paramsResource->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(3, freeListIndexUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(4, freeListUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(5, activeCountUavHandleGPU_);

	uint32_t requiredGroups = GPUParticle::GetRequiredThreadGroups();
	commandList->Dispatch(requiredGroups, 1, 1);

	//-----------------------------------------
	// 状態戻し
	//-----------------------------------------
	dxCommon_->TransitionBarrier(
		particleResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		freeListIndexResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		freeListResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);

}

#ifdef USE_IMGUI
/// <summary>
/// ImGuiで統計情報を表示
/// </summary>
void GPUParticle::DrawStatsImGui()
{
	ImGui::Begin("GPU Particle Statistics");

	ImGui::Text("=== Particle Configuration ===");
	ImGui::Text("Max Particles: %u", kMaxParticles);
	ImGui::Text("Particles Per Thread: %u", kParticlesPerThread);
	ImGui::Text("Thread Groups: %u", GetRequiredThreadGroups());

	ImGui::Separator();

	ImGui::Text("=== Current Status ===");

	if (!cachedStats_.isValid) {
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Stats not yet available...");
		ImGui::Text("Waiting for GPU data (3 frame latency)");
	} else {
		ImGui::Text("Active Particles: %u", cachedStats_.activeCount);
		ImGui::Text("Free Particles: %u", cachedStats_.freeCount);
		ImGui::Text("FreeList Index: %d", cachedStats_.freeListIndex);

		// プログレスバー
		ImGui::ProgressBar(
			cachedStats_.usagePercent / 100.0f,
			ImVec2(-1, 0),
			std::format("{:.1f}%", cachedStats_.usagePercent).c_str()
		);

		// 警告表示
		if (cachedStats_.freeListIndex < 0) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR: FreeList Exhausted!");
		} else if (cachedStats_.freeCount < 1000) {
			ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "WARNING: Low Free Particles!");
		}
	}

	ImGui::Separator();

	ImGui::Text("=== Performance ===");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

	ImGui::Separator();
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1, 0, 0, 1), "Causes GPU stall!");

	ImGui::End();
}
#endif

/// <summary>
/// テクスチャ読み込み＆SRV 取得
/// </summary>
void GPUParticle::CreateTexture()
{
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	textureIndexSRV_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

/// <summary>
/// Material の動的更新（現状未使用）
/// </summary>
void GPUParticle::UpdateMaterial()
{
}

/// <summary>
/// Light の動的更新（現状未使用）
/// </summary>
void GPUParticle::UpdateLight()
{
}

/// <summary>
/// PerView 更新（WVP・ビルボード行列計算）
/// </summary>
void GPUParticle::UpdatePerView()
{
	//-----------------------------------------
	// ViewProjection 更新
	//-----------------------------------------
	perViewData_->viewProjection = camera_->viewProjectionMatrix_;

	//-----------------------------------------
	// ビルボード行列計算
	//-----------------------------------------
	Matrix4x4 view = camera_->viewMatrix_;
	Matrix4x4 proj = camera_->projectionMatrix_;
	Matrix4x4 vp = Multiply(view, proj);

	Matrix4x4 billboard = view;
	billboard.m[3][0] = 0.0f;
	billboard.m[3][1] = 0.0f;
	billboard.m[3][2] = 0.0f;
	billboard.m[3][3] = 1.0f;

	Matrix4x4 billboardBase = Inverse(billboard);
	perViewData_->billboardMatrix = billboardBase;

	// エミッタ単位のビルボード ON/OFF を毎フレーム反映（切替が既存粒子含め即反映される）
	perViewData_->isBillboard = billboard_ ? 1u : 0u;
}
