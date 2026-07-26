#include "YGpuParticle.h"
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
void YGpuParticle::Initialize(const std::string& filepath, YoRigine::Camera* camera)
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
	CreateExtParamsResource();

	CreateUAV();
	CreateDrawIndirectResources();
	CreateYGpuParticleResource();
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
void YGpuParticle::Update(ID3D12Resource* resource, ID3D12Resource* paramsResource,
	D3D12_GPU_DESCRIPTOR_HANDLE forceFieldSrv, D3D12_GPU_DESCRIPTOR_HANDLE noiseFieldSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE accelerationFieldSrv, bool trailEnabled)
{
	UpdatePerView();
	DispatchUpdate(resource, paramsResource, forceFieldSrv, noiseFieldSrv, accelerationFieldSrv, trailEnabled);

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

	// cachedStats_ 側に反映（GPU側カウンタが万一壊れても表示が数十億に化けないようクランプ）
	const uint32_t clampedActive = std::min(cachedActiveCount_, kMaxParticles);
	cachedStats_.maxParticles = kMaxParticles;
	cachedStats_.activeCount = clampedActive;
	cachedStats_.freeCount = kMaxParticles - clampedActive;
	cachedStats_.usagePercent =
		(float)clampedActive / (float)kMaxParticles * 100.0f;
	cachedStats_.isValid = true;
}


/// <summary>
/// GPU パーティクル描画
/// </summary>
void YGpuParticle::Draw()
{
	auto commandList = dxCommon_->GetCommandList();

	//-----------------------------------------
	// パイプライン設定
	//-----------------------------------------
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("YGpuParticleInit");

	auto pso = YPipelineManager::GetInstance()->GetBlendModePSO("YGpuParticleInit",blendMode_);
	commandList->SetPipelineState(pso);
	commandList->SetGraphicsRootSignature(pipelineManager_->GetRootSignature("YGpuParticleInit"));

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
	commandList->SetGraphicsRootConstantBufferView(indices.at("g_ExtParams"), extParamsResource_->GetGPUVirtualAddress());
	// マテリアル
	materialUV_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialUV"));
	materialColor_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialColor"));
	// SoA: hot(t0)+warm(t1)+drawList(t2) を VS へ。生存スロットのみ描画・scale/colorは VS で lerp 導出
	commandList->SetGraphicsRootDescriptorTable(indices.at("g_Hot"), particleSrvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(indices.at("g_Warm"), warmSrvHandleGPU_);
	commandList->SetGraphicsRootDescriptorTable(indices.at("g_DrawList"), drawListSrvHandleGPU_);
	YoRigine::SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), textureIndexSRV_);

	//-----------------------------------------
	// インダイレクト描画: DrawList を VS 読み取り状態へ、DrawArgs を間接引数状態へ遷移し、
	// 生存粒子数（drawArgs[1]=InstanceCount）ぶんだけ描画する。
	//-----------------------------------------
	dxCommon_->TransitionBarrier(
		drawListResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		drawArgsResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
	);

	commandList->ExecuteIndirect(
		drawIndirectCmdSig_.Get(),
		1,                          // 最大コマンド数（DrawIndexedInstancedIndirect 1本）
		drawArgsResource_.Get(),    // 引数バッファ（先頭に D3D12_DRAW_INDEXED_ARGUMENTS）
		0,
		nullptr,
		0
	);
}

void YGpuParticle::Reset()
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
void YGpuParticle::CreateUAV()
{
	//-----------------------------------------
	// Hot UAV (u0)
	//-----------------------------------------
	uavIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	particleResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(ParticleHotGPU) * kMaxParticles);

	YoRigine::SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		uavIndex_,
		particleResource_.Get(),
		kMaxParticles,
		sizeof(ParticleHotGPU)
	);

	particleUavHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(uavIndex_);

	//-----------------------------------------
	// Warm UAV (u4)
	//-----------------------------------------
	warmUavIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	warmResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(ParticleWarmGPU) * kMaxParticles);

	YoRigine::SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		warmUavIndex_,
		warmResource_.Get(),
		kMaxParticles,
		sizeof(ParticleWarmGPU)
	);

	warmUavHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(warmUavIndex_);

	//-----------------------------------------
	// Cold UAV (u5)
	//-----------------------------------------
	coldUavIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	coldResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(ParticleColdGPU) * kMaxParticles);

	YoRigine::SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		coldUavIndex_,
		coldResource_.Get(),
		kMaxParticles,
		sizeof(ParticleColdGPU)
	);

	coldUavHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(coldUavIndex_);

	//-----------------------------------------
	// FreeListIndex UAV
	//-----------------------------------------
	freeListIndexUavIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	freeListIndexResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(int32_t));

	YoRigine::SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		freeListIndexUavIndex_,
		freeListIndexResource_.Get(),
		1,
		sizeof(int32_t)
	);

	freeListIndexUavHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(freeListIndexUavIndex_);

	//-----------------------------------------
	// FreeList UAV
	//-----------------------------------------
	freeListUavIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	freeListResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(uint32_t) * kMaxParticles);

	YoRigine::SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		freeListUavIndex_,
		freeListResource_.Get(),
		kMaxParticles,
		sizeof(uint32_t)
	);

	freeListUavHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(freeListUavIndex_);

	//-----------------------------------------
	// ActiveCount UAV
	//-----------------------------------------
	activeCountUavIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	activeCountResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(uint32_t));

	YoRigine::SrvManager::GetInstance()->CreateUAVForStructuredBuffer(
		activeCountUavIndex_,
		activeCountResource_.Get(),
		1,
		sizeof(uint32_t)
	);

	activeCountUavHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(activeCountUavIndex_);

}

/// <summary>
/// インダイレクト描画用リソース生成（生存粒子だけを描画するためのコンパクトリスト＋描画引数＋コマンドシグネチャ）
/// </summary>
void YGpuParticle::CreateDrawIndirectResources()
{
	auto* srv = YoRigine::SrvManager::GetInstance();

	//-----------------------------------------
	// DrawList UAV (u6) — 生存スロット番号を詰める uint[max]
	//-----------------------------------------
	drawListUavIndex_ = srv->Allocate();
	drawListResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(uint32_t) * kMaxParticles);
	srv->CreateUAVForStructuredBuffer(drawListUavIndex_, drawListResource_.Get(), kMaxParticles, sizeof(uint32_t));
	drawListUavHandleGPU_ = srv->GetGPUDescriptorHandle(drawListUavIndex_);

	// DrawList SRV (VS t2) — 同じバッファを VS が読む
	drawListSrvIndex_ = srv->Allocate();
	srv->CreateSRVforStructuredBuffer(drawListSrvIndex_, drawListResource_.Get(), kMaxParticles, sizeof(uint32_t));
	drawListSrvHandleGPU_ = srv->GetGPUDescriptorHandle(drawListSrvIndex_);

	//-----------------------------------------
	// DrawArgs UAV (u7) — D3D12_DRAW_INDEXED_ARGUMENTS(5 uint)。[1]=InstanceCount 兼 生存数カウンタ
	//-----------------------------------------
	drawArgsUavIndex_ = srv->Allocate();
	drawArgsResource_ = dxCommon_->CreateBufferResourceUAV(sizeof(uint32_t) * 5);
	srv->CreateUAVForStructuredBuffer(drawArgsUavIndex_, drawArgsResource_.Get(), 5, sizeof(uint32_t));
	drawArgsUavHandleGPU_ = srv->GetGPUDescriptorHandle(drawArgsUavIndex_);

	//-----------------------------------------
	// コマンドシグネチャ（DrawIndexedInstancedIndirect 1本）
	// ルートシグネチャは描画時に別途設定するので nullptr
	//-----------------------------------------
	D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
	argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC cmdDesc = {};
	cmdDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	cmdDesc.NumArgumentDescs = 1;
	cmdDesc.pArgumentDescs = &argDesc;

	HRESULT hr = dxCommon_->GetDevice()->CreateCommandSignature(
		&cmdDesc, nullptr, IID_PPV_ARGS(&drawIndirectCmdSig_));
	assert(SUCCEEDED(hr));
	(void)hr; // Release で assert が除去され未使用になるのを回避
}

/// <summary>
/// GPU パーティクルの SRV 作成（パーティクルを読み込む）
/// </summary>
void YGpuParticle::CreateYGpuParticleResource()
{
	// VS は hot(t0) と warm(t1) を読み、scale/color を lerp 導出する
	srvIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	YoRigine::SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
		srvIndex_,
		particleResource_.Get(),
		kMaxParticles,
		sizeof(ParticleHotGPU)
	);

	particleSrvHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);

	warmSrvIndex_ = YoRigine::SrvManager::GetInstance()->Allocate();

	YoRigine::SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
		warmSrvIndex_,
		warmResource_.Get(),
		kMaxParticles,
		sizeof(ParticleWarmGPU)
	);

	warmSrvHandleGPU_ = YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(warmSrvIndex_);
}

/// <summary>
/// PerView用 CB 作成
/// </summary>
void YGpuParticle::CreatePerViewResource()
{
	perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerViewForGPU));
	perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));

	perViewData_->viewProjection = MakeIdentity4x4();
	perViewData_->billboardMatrix = MakeIdentity4x4();
	perViewData_->isBillboard = billboard_ ? 1u : 0u;
}

/// <summary>
/// 拡張Paramモジュール(UVScroll/ScalePulse/ColorFlicker)用 CB 作成
/// </summary>
void YGpuParticle::CreateExtParamsResource()
{
	extParamsResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleExtParameters));
	extParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&extParamsData_));
	*extParamsData_ = ParticleExtParameters{}; // 既定: 全モジュール無効
}

/// <summary>
/// パーティクル描画用の頂点（板ポリ）
/// </summary>
void YGpuParticle::CreateVertexResource()
{
	mesh_ = MeshPrimitive::CreatePlane(1.0f, 1.0f);
}

/// <summary>
/// 初期化用 ComputeShader の実行
/// </summary>
void YGpuParticle::DispatchInit()
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
	// SoA warm/cold も hot と同じく書き込み対象
	dxCommon_->TransitionBarrier(
		warmResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		coldResource_.Get(),
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

	ID3D12DescriptorHeap* heaps[] = { YoRigine::SrvManager::GetInstance()->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	commandList->SetComputeRootDescriptorTable(0, particleUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(1, freeListIndexUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(2, freeListUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(3, activeCountUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(4, warmUavHandleGPU_); // u4
	commandList->SetComputeRootDescriptorTable(5, coldUavHandleGPU_); // u5

	uint32_t requiredGroups = YGpuParticle::GetRequiredThreadGroups();
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
		warmResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		coldResource_.Get(),
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

	// インダイレクト描画バッファを静止状態へ（DrawList=VS読み取り / DrawArgs=間接引数）。
	// 中身は毎フレーム DispatchUpdate 冒頭で作り直すのでここでは状態のみ確定させる。
	dxCommon_->TransitionBarrier(
		drawListResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		drawArgsResource_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
	);
}

/// <summary>
/// Update 用 ComputeShader 実行（Emit されたパーティクルを更新）
/// </summary>
void YGpuParticle::DispatchUpdate(ID3D12Resource* resource, ID3D12Resource* paramsResource,
	D3D12_GPU_DESCRIPTOR_HANDLE forceFieldSrv, D3D12_GPU_DESCRIPTOR_HANDLE noiseFieldSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE accelerationFieldSrv, bool trailEnabled)
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
	// warm/cold は TrailSpawn パスが読み書きする（Update 本体は hot のみ触る）
	dxCommon_->TransitionBarrier(
		warmResource_.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		coldResource_.Get(),
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
	// インダイレクト描画バッファも書き込み対象へ（DrawList=VS読み取り→UAV / DrawArgs=間接引数→UAV）
	dxCommon_->TransitionBarrier(
		drawListResource_.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxCommon_->TransitionBarrier(
		drawArgsResource_.Get(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	ID3D12DescriptorHeap* heaps[] = { YoRigine::SrvManager::GetInstance()->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	//-----------------------------------------
	// 描画引数リセット（1スレッド）: InstanceCount=0 / IndexCountPerInstance=索引数 に初期化。
	// この後 Update CS が生存粒子を InterlockedAdd で数え上げる。
	//-----------------------------------------
	commandList->SetComputeRootSignature(computeShaderManager_->GetRootSignature("ResetDrawArgsCS"));
	commandList->SetPipelineState(computeShaderManager_->GetComputePipelineState("ResetDrawArgsCS"));
	commandList->SetComputeRootDescriptorTable(0, drawArgsUavHandleGPU_);
	commandList->SetComputeRoot32BitConstant(1, static_cast<UINT>(mesh_->GetIndexCount()), 0);
	commandList->Dispatch(1, 1, 1);
	dxCommon_->BarrierTypeUAV(drawArgsResource_.Get());

	//-----------------------------------------
	// CS Pipeline
	//-----------------------------------------
	commandList->SetComputeRootSignature(computeShaderManager_->GetRootSignature("ParticleUpdateCS"));
	commandList->SetPipelineState(computeShaderManager_->GetComputePipelineState("ParticleUpdateCS"));

	commandList->SetComputeRootDescriptorTable(0, particleUavHandleGPU_);
	commandList->SetComputeRootConstantBufferView(1, resource->GetGPUVirtualAddress());
	commandList->SetComputeRootConstantBufferView(2, paramsResource->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(3, freeListIndexUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(4, freeListUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(5, activeCountUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(6, forceFieldSrv); // ForceFields SRV t0
	// warm(u4)/cold(u5) は TrailSpawnCS 用（Update 本体は未使用だが同一 RS なのでバインド）
	commandList->SetComputeRootDescriptorTable(7, warmUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(8, coldUavHandleGPU_);
	// drawList(u6)/drawArgs(u7): Update CS が生存粒子を追記する
	commandList->SetComputeRootDescriptorTable(9, drawListUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(10, drawArgsUavHandleGPU_);
	commandList->SetComputeRootDescriptorTable(11, noiseFieldSrv); // NoiseFields SRV t1 (末尾追加)
	commandList->SetComputeRootDescriptorTable(12, accelerationFieldSrv); // AccelerationFields SRV t2 (末尾追加)
	// ExtParams CBV b2: Drag/Bounce を CS でも読む（VS は b1 で同じリソースを見る）
	commandList->SetComputeRootConstantBufferView(13, extParamsResource_->GetGPUVirtualAddress());

	uint32_t requiredGroups = YGpuParticle::GetRequiredThreadGroups();
	commandList->Dispatch(requiredGroups, 1, 1);

	//-----------------------------------------
	// トレイル生成パス（FreeList Pop 専用）
	// Update パスの死亡処理（Push）と同一ディスパッチで Pop すると
	// FreeList が競合破壊されるため、UAVバリアで完了を待ってから別パスで実行する
	//-----------------------------------------
	if (trailEnabled) {
		dxCommon_->BarrierTypeUAV(particleResource_.Get());
		dxCommon_->BarrierTypeUAV(warmResource_.Get());
		dxCommon_->BarrierTypeUAV(coldResource_.Get());
		dxCommon_->BarrierTypeUAV(freeListIndexResource_.Get());
		dxCommon_->BarrierTypeUAV(freeListResource_.Get());
		dxCommon_->BarrierTypeUAV(activeCountResource_.Get());

		// ルートシグネチャは ParticleUpdateCS と共用（バインド済みの内容をそのまま使う）
		commandList->SetPipelineState(computeShaderManager_->GetComputePipelineState("TrailSpawnCS"));
		commandList->Dispatch(requiredGroups, 1, 1);
	}

	//-----------------------------------------
	// 状態戻し
	//-----------------------------------------
	dxCommon_->TransitionBarrier(
		particleResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		warmResource_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dxCommon_->TransitionBarrier(
		coldResource_.Get(),
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
void YGpuParticle::DrawStatsImGui()
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
void YGpuParticle::CreateTexture()
{
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	textureIndexSRV_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
}

/// <summary>
/// 実行時のテクスチャ差し替え（YParticleSystem::SetTexture と同じパターン）
/// </summary>
void YGpuParticle::SetTexture(const std::string& textureFilePath)
{
	textureFilePath_ = textureFilePath;

	if (!textureFilePath.empty()) {
		TextureManager::GetInstance()->LoadTexture(textureFilePath);
		textureIndexSRV_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
	}
}

/// <summary>
/// Material の動的更新（現状未使用）
/// </summary>
void YGpuParticle::UpdateMaterial()
{
}

/// <summary>
/// Light の動的更新（現状未使用）
/// </summary>
void YGpuParticle::UpdateLight()
{
}

/// <summary>
/// PerView 更新（WVP・ビルボード行列計算）
/// </summary>
void YGpuParticle::UpdatePerView()
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
