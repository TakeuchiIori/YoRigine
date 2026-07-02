#include "GPUEmitter.h"

// Engine
#include <Systems/GameTime/GameTime.h>
#include <ComputeShaderManager/ComputeShaderManager.h>
#include <ModelUtils.h>
#include "GpuParticleMethod.h"
#include <Debugger/Logger.h>

/// <summary>
/// GPU エミッターの初期化（パーティクル生成・各種リソース作成）
/// </summary>
void GPUEmitter::Initialize(Camera* camera, std::string& texturePath)
{
	camera_ = camera;

	// GPU パーティクル本体
	gpuParticle_ = std::make_unique<GPUParticle>();
	gpuParticle_->Initialize(texturePath, camera_);

	CreateEmitterResources();
	CreatePerFrameResource();
	CreateParticleParametersResource();

	ParticleParams defaultParams{};
	defaultParams.lifeTime = 3.0f;
	defaultParams.lifeTimeVariance = 0.5f;

	defaultParams.startScale = Vector3(1.0f, 1.0f, 1.0f);
	defaultParams.startScaleVariance = Vector3(0.3f, 0.3f, 0.3f);
	defaultParams.endScale = Vector3(0.0f, 0.0f, 0.0f);
	defaultParams.endScaleVariance = Vector3(0.3f, 0.3f, 0.3f);

	defaultParams.rotation = 0.0f;
	defaultParams.rotationVariance = 0.0f;
	defaultParams.rotationSpeed = 0.0f;
	defaultParams.rotationSpeedVariance = 0.0f;

	defaultParams.velocity = Vector3(0.0f, 0.1f, 0.0f);
	defaultParams.velocityVariance = Vector3(0.1f, 0.05f, 0.1f);

	defaultParams.startColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	defaultParams.startColorVariance = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	defaultParams.endColor = Vector4(1.0f, 1.0f, 1.0f, 0.0f);
	defaultParams.endColorVariance = Vector4(0.0f, 0.0f, 0.0f, 0.0f);

	defaultParams.gravity = 0.0f;
	defaultParams.isBillboard = 1;
	SetParticleParameters(defaultParams);

	//-----------------------------------------
	// デフォルト形状（Cone）を設定
	//-----------------------------------------
	SetEmitterShape(EmitterShape::Cone);
	SetConeParams(
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, 1.0f, 0.0f),
		10.0f,
		20.0f,
		100.0f,
		1.0f
	);
}

/// <summary>
/// エミッター更新処理（形状ごとに emit を制御）
/// </summary>
void GPUEmitter::Update()
{
	//-----------------------------------------
	// 共通データの更新
	//-----------------------------------------
	emitterCommonData_->emitterShape = static_cast<uint32_t>(currentShape_);
	perframeData_->time = YoRigine::GameTime::GetTotalTime();
	perframeData_->deltaTime = YoRigine::GameTime::GetUnscaledDeltaTime();

	// エミッターの更新
	UpdateEmitters();

	// トレイルの更新
	UpdateEmitterTrail();

	//-----------------------------------------
	// パーティクルのレンダリング更新
	//-----------------------------------------
	gpuParticle_->Update(perframeResource_.Get(), particleParametersResource_.Get());

	//-----------------------------------------
	// ComputeShader 実行
	//-----------------------------------------
	Dispatch();
}

/// <summary>
/// GPU パーティクル描画
/// </summary>
void GPUEmitter::Draw()
{
	gpuParticle_->Draw();
}

void GPUEmitter::Reset()
{
	if (gpuParticle_) {
		gpuParticle_->Reset();
	}

	timeScalelastEmit_ = 0.0f;
	burstRequest_ = 0;
	continuousEmit_ = false;
}

/// <summary>
/// 使用するエミッター形状を変更
/// </summary>
void GPUEmitter::SetEmitterShape(EmitterShape shape)
{
	currentShape_ = shape;
}

/// <summary>
/// Sphere パラメータ設定
/// </summary>
void GPUEmitter::SetSphereParams(const Vector3& translate, float radius, float count, float emitInterval)
{
	if (!emitterSphereData_) return;

	emitterSphereData_->translate = translate;
	emitterSphereData_->radius = radius;
	emitterSphereData_->count = count;
	emitterSphereData_->emitInterval = emitInterval;
	emitterSphereData_->intervalTime = 0.0f;
	emitterSphereData_->isEmit = 1;
}

/// <summary>
/// Box パラメータ設定
/// </summary>
void GPUEmitter::SetBoxParams(const Vector3& translate, const Vector3& size, float count, float emitInterval)
{
	if (!emitterBoxData_) return;

	emitterBoxData_->translate = translate;
	emitterBoxData_->size = size;
	emitterBoxData_->count = count;
	emitterBoxData_->emitInterval = emitInterval;
	emitterBoxData_->intervalTime = 0.0f;
	emitterBoxData_->isEmit = 1;
}

/// <summary>
/// Triangle パラメータ設定
/// </summary>
void GPUEmitter::SetTriangleParams(const Vector3& v1, const Vector3& v2, const Vector3& v3,
	const Vector3& translate, float count, float emitInterval)
{
	if (!emitterTriangleData_) return;

	emitterTriangleData_->v1 = v1;
	emitterTriangleData_->v2 = v2;
	emitterTriangleData_->v3 = v3;
	emitterTriangleData_->translate = translate;
	emitterTriangleData_->count = count;
	emitterTriangleData_->emitInterval = emitInterval;
	emitterTriangleData_->intervalTime = 0.0f;
	emitterTriangleData_->isEmit = 1;
}

/// <summary>
/// Cone パラメータ設定
/// </summary>
void GPUEmitter::SetConeParams(const Vector3& translate, const Vector3& direction, float radius,
	float height, float count, float emitInterval)
{
	if (!emitterConeData_) return;

	emitterConeData_->translate = translate;
	emitterConeData_->direction = direction;
	emitterConeData_->radius = radius;
	emitterConeData_->height = height;
	emitterConeData_->count = count;
	emitterConeData_->emitInterval = emitInterval;
	emitterConeData_->intervalTime = 0.0f;
	emitterConeData_->isEmit = 1;
}

void GPUEmitter::SetMeshParams(Model* model, const Vector3& translate, const Vector3& scale, const Quaternion& rotation, float count, float emitInterval, MeshEmitMode mode)
{
	if (!emitterMeshData_ || !model) return;

	currentMeshModel_ = model;
	currentMeshMode_ = mode;

	emitterMeshData_->translate = translate;
	emitterMeshData_->scale = scale;
	emitterMeshData_->rotation = Vector4(rotation.x, rotation.y, rotation.z, rotation.w);
	emitterMeshData_->count = count;
	emitterMeshData_->emitInterval = emitInterval;
	emitterMeshData_->intervalTime = 0.0f;
	emitterMeshData_->isEmit = 1;
	emitterMeshData_->emitMode = static_cast<uint32_t>(mode);

	// メッシュの三角形バッファを作成
	CreateMeshTriangleBuffer();
	// メッシュの三角形データを更新
	UpdateMeshTriangleData(model);
	emitterMeshData_->triangleCount = static_cast<uint32_t>(meshTriangles_.size());
}

//-----------------------------------------
// UpdateXXXParams は初期化時との差分だけ更新
//-----------------------------------------

void GPUEmitter::UpdateSphereParams(const Vector3& translate, float radius, float count, float emitInterval)
{
	if (!emitterSphereData_) return;

	emitterSphereData_->translate = translate;
	emitterSphereData_->radius = radius;
	emitterSphereData_->count = count;
	emitterSphereData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateBoxParams(const Vector3& translate, const Vector3& size, float count, float emitInterval)
{
	if (!emitterBoxData_) return;

	emitterBoxData_->translate = translate;
	emitterBoxData_->size = size;
	emitterBoxData_->count = count;
	emitterBoxData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateTriangleParams(const Vector3& v1, const Vector3& v2, const Vector3& v3,
	const Vector3& translate, float count, float emitInterval)
{
	if (!emitterTriangleData_) return;

	emitterTriangleData_->v1 = v1;
	emitterTriangleData_->v2 = v2;
	emitterTriangleData_->v3 = v3;
	emitterTriangleData_->translate = translate;
	emitterTriangleData_->count = count;
	emitterTriangleData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateConeParams(const Vector3& translate, const Vector3& direction, float radius,
	float height, float count, float emitInterval)
{
	if (!emitterConeData_) return;

	emitterConeData_->translate = translate;
	emitterConeData_->direction = direction;
	emitterConeData_->radius = radius;
	emitterConeData_->height = height;
	emitterConeData_->count = count;
	emitterConeData_->emitInterval = emitInterval;
}

void GPUEmitter::UpdateMeshParams(Model* model, const Vector3& translate, const Vector3& scale, const Quaternion& rotation, float count, float emitInterval, MeshEmitMode mode)
{
	if (!emitterMeshData_) return;

	if (model != currentMeshModel_) {
		// モデルが変わった場合は再設定
		SetMeshParams(model, translate, scale, rotation, count, emitInterval, mode);
		return;
	}

	emitterMeshData_->translate = translate;
	emitterMeshData_->scale = scale;
	emitterMeshData_->rotation = Vector4(rotation.x, rotation.y, rotation.z, rotation.w);
	emitterMeshData_->count = count;
	emitterMeshData_->emitInterval = emitInterval;
	emitterMeshData_->emitMode = static_cast<uint32_t>(mode);
	currentMeshMode_ = mode;
}

void GPUEmitter::SetParticleParameters(const ParticleParams& params)
{
	if (particleParameters_) {
		particleParameters_->lifeTime = params.lifeTime;
		particleParameters_->lifeTimeVariance = params.lifeTimeVariance;

		particleParameters_->startScale = params.startScale;
		particleParameters_->startScaleVariance = params.startScaleVariance;
		particleParameters_->endScale = params.endScale;
		particleParameters_->endScaleVariance = params.endScaleVariance;

		particleParameters_->rotation = params.rotation;
		particleParameters_->rotationVariance = params.rotationVariance;
		particleParameters_->rotationSpeed = params.rotationSpeed;
		particleParameters_->rotationSpeedVariance = params.rotationSpeedVariance;

		particleParameters_->velocity = params.velocity;
		particleParameters_->velocityVariance = params.velocityVariance;

		particleParameters_->startColor = params.startColor;
		particleParameters_->startColorVariance = params.startColorVariance;
		particleParameters_->endColor = params.endColor;
		particleParameters_->endColorVariance = params.endColorVariance;

		particleParameters_->gravity = params.gravity;
		particleParameters_->isBillboard = params.isBillboard ? 1 : 0;

		// 子パーティクルパラメータ
		particleParameters_->childParams.isTrail = params.child.isTrail ? 1 : 0;
		particleParameters_->childParams.minDistance = params.child.minDistance;
		particleParameters_->childParams.lifeTime = params.child.lifeTime;
		particleParameters_->childParams.emissionCount = static_cast<int>(params.child.emissionCount);
		particleParameters_->childParams.inheritScale = params.child.isInheritScale ? 1 : 0;
		particleParameters_->childParams.startScale = params.child.startScale;
		particleParameters_->childParams.endScale = params.child.endScale;

	}
}

void GPUEmitter::EmitAtPosition(const Vector3& position, float count)
{
	lastEmitWorldPos_ = position;
	hasLastEmitWorldPos_ = true;

	// translate と count のみ設定。実際の発生は次の Update()→UpdateEmitters() で
	// burstRequest_ を消費して1回だけ行う（isEmit の直接設定は UpdateEmitters に上書きされるため使わない）
	switch (currentShape_) {
	case EmitterShape::Sphere:
		emitterSphereData_->translate = position;
		emitterSphereData_->count = count;
		break;

	case EmitterShape::Box:
		emitterBoxData_->translate = position;
		emitterBoxData_->count = count;
		break;

	case EmitterShape::Triangle:
		emitterTriangleData_->translate = position;
		emitterTriangleData_->count = count;
		break;

	case EmitterShape::Cone:
		emitterConeData_->translate = position;
		emitterConeData_->count = count;
		break;

	case EmitterShape::Mesh:
		emitterMeshData_->translate = position;
		emitterMeshData_->count = count;
		break;
	}

	// 次フレームに1回だけ発生
	RequestBurst();
}

/// <summary>
/// 各エミッター形状の GPU リソース（ConstantBuffer）を生成
/// </summary>
void GPUEmitter::CreateEmitterResources()
{
	auto* dx = YoRigine::DirectXCommon::GetInstance();

	//-----------------------------------------
	// 共通データ
	//-----------------------------------------
	emitterCommonResource_ = dx->CreateBufferResource(sizeof(EmitterCommonData));
	emitterCommonResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterCommonData_));
	emitterCommonData_->emitterShape = static_cast<uint32_t>(EmitterShape::Sphere);

	//-----------------------------------------
	// Sphere
	//-----------------------------------------
	emitterSphereResource_ = dx->CreateBufferResource(sizeof(EmitterSphereData));
	emitterSphereResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterSphereData_));

	//-----------------------------------------
	// Box
	//-----------------------------------------
	emitterBoxResource_ = dx->CreateBufferResource(sizeof(EmitterBoxData));
	emitterBoxResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterBoxData_));

	//-----------------------------------------
	// Triangle
	//-----------------------------------------
	emitterTriangleResource_ = dx->CreateBufferResource(sizeof(EmitterTriangleData));
	emitterTriangleResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterTriangleData_));

	//-----------------------------------------
	// Cone
	//-----------------------------------------
	emitterConeResource_ = dx->CreateBufferResource(sizeof(EmitterConeData));
	emitterConeResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterConeData_));

	//-----------------------------------------
	// Mesh（新規追加）
	//-----------------------------------------
	emitterMeshResource_ = dx->CreateBufferResource(sizeof(EmitterMeshData));
	emitterMeshResource_->Map(0, nullptr, reinterpret_cast<void**>(&emitterMeshData_));

}

void GPUEmitter::CreateParticleParametersResource()
{
	auto* dx = YoRigine::DirectXCommon::GetInstance();
	particleParametersResource_ = dx->CreateBufferResource(sizeof(ParticleParameters));
	particleParametersResource_->Map(0, nullptr, reinterpret_cast<void**>(&particleParameters_));
}

/// <summary>
/// 毎フレーム用定数バッファ作成（時間・デルタ）
/// </summary>
void GPUEmitter::CreatePerFrameResource()
{
	auto* dx = YoRigine::DirectXCommon::GetInstance();

	perframeResource_ = dx->CreateBufferResource(sizeof(PerFrameData));
	perframeResource_->Map(0, nullptr, reinterpret_cast<void**>(&perframeData_));
}

void GPUEmitter::CreateMeshTriangleBuffer()
{
	auto* dx = YoRigine::DirectXCommon::GetInstance();
	if (meshTriangleBuffer_) {
		return;
	}

	// 最大三角形数分のバッファを確保
	size_t bufferSize = sizeof(MeshTriangle) * kMaxTriangles_;

	meshTriangleBuffer_ = dx->CreateBufferResource(bufferSize);
	meshTriangleBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&meshTriangleData_));

	// SRV作成
	auto* srvManager = SrvManager::GetInstance();
	meshTriangleBufferSrvIndex_ = srvManager->Allocate();

	srvManager->CreateSRVforStructuredBuffer(
		meshTriangleBufferSrvIndex_,
		meshTriangleBuffer_.Get(),
		static_cast<UINT>(kMaxTriangles_),
		sizeof(MeshTriangle)
	);
}
/// <summary>
/// モデルからメッシュ三角形情報を収集してGPUバッファへ転送
/// </summary>
void GPUEmitter::UpdateMeshTriangleData(Model* model)
{
	if (!model || !meshTriangleData_) {
		return;
	}

	// ★ メモリ節約: reserve で事前確保
	meshTriangles_.clear();
	meshTriangles_.reserve(std::min<size_t>(10000, kMaxTriangles_)); // 初期確保を控えめに

	// エッジマップ: ★ reserve で事前確保してメモリ再確保を減らす
	std::map<EdgeKey, std::vector<size_t>> edgeMap;

	const auto& meshes = model->GetMeshes();

	// 三角形の概算数を計算
	size_t estimatedTriangles = 0;
	for (const auto& meshPtr : meshes) {
		if (meshPtr) {
			estimatedTriangles += meshPtr->GetMeshData().indices.size() / 3;
		}
	}

	// ★ 上限チェック: 巨大メッシュの場合は警告
	if (estimatedTriangles > kMaxTriangles_) {
		Logger("Warning: Mesh has " + std::to_string(estimatedTriangles) +
			" triangles, limiting to " + std::to_string(kMaxTriangles_));
	}

	// 1. 全三角形を走査してリスト化 & エッジ登録
	for (const auto& meshPtr : meshes)
	{
		if (!meshPtr) continue;

		const Mesh::MeshData& meshData = meshPtr->GetMeshData();
		const auto& vertices = meshData.vertices;
		const auto& indices = meshData.indices;

		size_t indexCount = indices.size();
		for (size_t i = 0; i + 2 < indexCount; i += 3)
		{
			// ★ 早期脱出: 上限に達したら即座に終了
			if (meshTriangles_.size() >= kMaxTriangles_) {
				goto end_triangle_loop;
			}

			MeshTriangle tri{};

			// 頂点取得
			auto p0 = vertices[indices[i + 0]].position;
			auto p1 = vertices[indices[i + 1]].position;
			auto p2 = vertices[indices[i + 2]].position;

			tri.v0 = { p0.x, p0.y, p0.z };
			tri.v1 = { p1.x, p1.y, p1.z };
			tri.v2 = { p2.x, p2.y, p2.z };

			// 法線と面積
			Vector3 edge1 = tri.v1 - tri.v0;
			Vector3 edge2 = tri.v2 - tri.v0;
			tri.normal = Normalize(Cross(edge1, edge2));
			tri.area = Length(Cross(edge1, edge2)) * 0.5f;

			// 初期状態は全エッジ有効
			tri.activeEdges = 7;

			// 現在の三角形インデックス
			size_t currentTriIndex = meshTriangles_.size();

			// エッジをマップに登録
			edgeMap[EdgeKey(tri.v0, tri.v1)].push_back(currentTriIndex);
			edgeMap[EdgeKey(tri.v1, tri.v2)].push_back(currentTriIndex);
			edgeMap[EdgeKey(tri.v2, tri.v0)].push_back(currentTriIndex);

			meshTriangles_.push_back(tri);
		}
	}

end_triangle_loop:

	// 2. マップを使って隣接判定
	for (const auto& pair : edgeMap)
	{
		const std::vector<size_t>& sharedTriangles = pair.second;

		// 同じエッジを共有する三角形が2つある場合
		if (sharedTriangles.size() == 2)
		{
			size_t idxA = sharedTriangles[0];
			size_t idxB = sharedTriangles[1];

			MeshTriangle& triA = meshTriangles_[idxA];
			MeshTriangle& triB = meshTriangles_[idxB];

			// 法線の内積
			float dot = Dot(triA.normal, triB.normal);

			// 法線がほぼ同じ向きなら内部エッジ
			if (dot > 0.99f)
			{
				const EdgeKey& key = pair.first;

				auto DisableEdge = [&](MeshTriangle& t, const EdgeKey& k) {
					Vector3 keyMid = {
						(k.p1.x + k.p2.x) * 0.5f,
						(k.p1.y + k.p2.y) * 0.5f,
						(k.p1.z + k.p2.z) * 0.5f
					};

					auto check = [&](Vector3 a, Vector3 b, int bit) {
						Vector3 edgeMid = (a + b) * 0.5f;
						float distSq = (edgeMid.x - keyMid.x) * (edgeMid.x - keyMid.x) +
							(edgeMid.y - keyMid.y) * (edgeMid.y - keyMid.y) +
							(edgeMid.z - keyMid.z) * (edgeMid.z - keyMid.z);
						if (distSq < 0.0001f) {
							t.activeEdges &= ~(1 << bit);
						}
						};

					check(t.v0, t.v1, 0);
					check(t.v1, t.v2, 1);
					check(t.v2, t.v0, 2);
					};

				DisableEdge(triA, key);
				DisableEdge(triB, key);
			}
		}
	}

	// 3. GPUバッファに転送
	size_t copyCount = std::min(meshTriangles_.size(), static_cast<size_t>(kMaxTriangles_));
	memcpy(meshTriangleData_, meshTriangles_.data(), copyCount * sizeof(MeshTriangle));

	// ★ メモリ解放: edgeMapのデータは不要なので即座にクリア
	edgeMap.clear();

	// ★ meshTriangles_も縮小（GPUに転送済み）
	meshTriangles_.shrink_to_fit();

	// エミッター側のカウント更新
	if (emitterMeshData_) {
		emitterMeshData_->triangleCount = static_cast<uint32_t>(meshTriangles_.size());
	}
}

/// <summary>
/// ComputeShader “EmitCS” を用いてパーティクルを生成
/// </summary>
void GPUEmitter::Dispatch()
{
	auto* dx = YoRigine::DirectXCommon::GetInstance();
	auto* cmd = dx->GetCommandList().Get();

	//-----------------------------------------
	// UAV バリア（書き込み準備）
	//-----------------------------------------
	dx->TransitionBarrier(
		gpuParticle_->GetParticleResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	dx->TransitionBarrier(
		gpuParticle_->GetFreeListIndexResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	dx->TransitionBarrier(
		gpuParticle_->GetFreeListResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	//-----------------------------------------
	// CS パイプライン設定
	//-----------------------------------------
	cmd->SetComputeRootSignature(ComputeShaderManager::GetInstance()->GetRootSignature("EmitCS"));
	cmd->SetPipelineState(ComputeShaderManager::GetInstance()->GetComputePipelineState("EmitCS"));

	ID3D12DescriptorHeap* heaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
	cmd->SetDescriptorHeaps(_countof(heaps), heaps);

	//-----------------------------------------
	// RootCBV 設定
	//-----------------------------------------
	cmd->SetComputeRootConstantBufferView(0, emitterCommonResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(1, emitterSphereResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(2, emitterBoxResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(3, emitterTriangleResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(4, emitterConeResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(5, emitterMeshResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(6, perframeResource_->GetGPUVirtualAddress());
	cmd->SetComputeRootConstantBufferView(7, particleParametersResource_->GetGPUVirtualAddress());

	//-----------------------------------------
	// UAV テーブル
	//-----------------------------------------
	cmd->SetComputeRootDescriptorTable(8, gpuParticle_->GetParticleUavHandleGPU());
	cmd->SetComputeRootDescriptorTable(9, gpuParticle_->GetFreeListIndexUavHandleGPU());
	cmd->SetComputeRootDescriptorTable(10, gpuParticle_->GetFreeListUavHandleGPU());
	cmd->SetComputeRootDescriptorTable(11, gpuParticle_->GetActiveCountUavHandleGPU());

	//-----------------------------------------
	// Mesh用のSRV設定（新規追加）
	//-----------------------------------------
	if (currentShape_ == EmitterShape::Mesh) {
		auto* srvManager = SrvManager::GetInstance();
		cmd->SetComputeRootDescriptorTable(12,
			srvManager->GetGPUDescriptorHandle(meshTriangleBufferSrvIndex_));
	}

	//-----------------------------------------
	// Emit 数を形状ごとに決定
	//-----------------------------------------
	uint32_t emitCount = 0;

	switch (currentShape_)
	{
	case EmitterShape::Sphere:
		emitCount = static_cast<uint32_t>(emitterSphereData_->count);
		break;

	case EmitterShape::Box:
		emitCount = static_cast<uint32_t>(emitterBoxData_->count);
		break;

	case EmitterShape::Triangle:
		emitCount = static_cast<uint32_t>(emitterTriangleData_->count);
		break;

	case EmitterShape::Cone:
		emitCount = static_cast<uint32_t>(emitterConeData_->count);
		break;
	case EmitterShape::Mesh:
		emitCount = static_cast<uint32_t>(emitterMeshData_->count);
		break;
	}

	//-----------------------------------------
	// Dispatch（スレッドグループ計算）
	//-----------------------------------------
	uint32_t groupX = (emitCount + threadsPerGroup_ - 1) / threadsPerGroup_;
	if (groupX == 0) groupX = 1;

	cmd->Dispatch(groupX, 1, 1);

	//-----------------------------------------
	// UAV → VERTEX&CB に戻す
	//-----------------------------------------
	dx->TransitionBarrier(
		gpuParticle_->GetParticleResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dx->TransitionBarrier(
		gpuParticle_->GetFreeListIndexResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);
	dx->TransitionBarrier(
		gpuParticle_->GetFreeListResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);


}

void GPUEmitter::UpdateEmitters()
{
	//-----------------------------------------
	// 現在の形状ごとに Emit 制御
	//-----------------------------------------
	// このフレームにワンショットのバースト要求があるか（continuousEmit_ と独立に1回だけ発生）
	const bool burst = (burstRequest_ > 0);
	if (burst) --burstRequest_;

	const float dt = YoRigine::GameTime::GetUnscaledDeltaTime();

	switch (currentShape_)
	{
	case EmitterShape::Sphere:
	{
		emitterSphereData_->intervalTime += dt;
		const bool intervalHit = emitterSphereData_->intervalTime >= emitterSphereData_->emitInterval;
		emitterSphereData_->isEmit = (burst || (continuousEmit_ && intervalHit)) ? 1 : 0;
		if (emitterSphereData_->isEmit) emitterSphereData_->intervalTime = 0.0f;
		break;
	}
	case EmitterShape::Box:
	{
		emitterBoxData_->intervalTime += dt;
		const bool intervalHit = emitterBoxData_->intervalTime >= emitterBoxData_->emitInterval;
		emitterBoxData_->isEmit = (burst || (continuousEmit_ && intervalHit)) ? 1 : 0;
		if (emitterBoxData_->isEmit) emitterBoxData_->intervalTime = 0.0f;
		break;
	}
	case EmitterShape::Triangle:
	{
		emitterTriangleData_->intervalTime += dt;
		const bool intervalHit = emitterTriangleData_->intervalTime >= emitterTriangleData_->emitInterval;
		emitterTriangleData_->isEmit = (burst || (continuousEmit_ && intervalHit)) ? 1 : 0;
		if (emitterTriangleData_->isEmit) emitterTriangleData_->intervalTime = 0.0f;
		break;
	}
	case EmitterShape::Cone:
	{
		emitterConeData_->intervalTime += dt;
		const bool intervalHit = emitterConeData_->intervalTime >= emitterConeData_->emitInterval;
		emitterConeData_->isEmit = (burst || (continuousEmit_ && intervalHit)) ? 1 : 0;
		if (emitterConeData_->isEmit) emitterConeData_->intervalTime = 0.0f;
		break;
	}
	case EmitterShape::Mesh:
	{
		emitterMeshData_->intervalTime += dt;
		const bool intervalHit = emitterMeshData_->intervalTime >= emitterMeshData_->emitInterval;
		emitterMeshData_->isEmit = (burst || (continuousEmit_ && intervalHit)) ? 1 : 0;
		if (emitterMeshData_->isEmit) emitterMeshData_->intervalTime = 0.0f;
		break;
	}
	}
}
void GPUEmitter::UpdateEmitterTrail()
{
	// ================================
	// Trail の適用
	// ================================
	auto& trail = trail_;
	if (!trail.isTrail) return;

	// 今のエミッター位置
	Vector3 current = GetEmitterPosition();

	// まだ基準位置が無いなら「覚えるだけ」で終わり（出さない）
	if (!trailHasLast_) {
		trailLastPos_ = current;
		trailHasLast_ = true;
		return;
	}

	// 前回からどれくらい動いたか
	Vector3 delta = current - trailLastPos_;
	float dist = Length(delta);

	// 全く動いてないなら出さない
	if (dist <= 0.0f) {
		return;
	}

	// -------------------------------
	// しきい値 0 以下 → 「ちょっとでも動いたら出す」モード
	// -------------------------------
	if (trail.minDistance <= 0.0f) {
		EmitAtPosition(current, trail.emissionCount);
		trailLastPos_ = current;

		if (trail.lifeTime > 0.0f) {
			particleParameters_->lifeTime = trail.lifeTime;
		}
		return;
	}

	// -------------------------------
	// 通常モード：一定距離ごとに出す
	// -------------------------------
	if (dist < trail.minDistance) {
		// まだ閾値未満なので何もしない
		return;
	}

	// 高速移動対策：dist が大きい場合は補間して複数発生
	int steps = static_cast<int>(dist / trail.minDistance);
	if (steps < 1) {
		steps = 1;
	}

	Vector3 step = delta / static_cast<float>(steps);
	Vector3 pos = trailLastPos_;

	for (int i = 0; i < steps; ++i) {
		pos += step;
		EmitAtPosition(pos, trail.emissionCount);
	}

	trailLastPos_ = current;

	if (trail.lifeTime > 0.0f) {
		particleParameters_->lifeTime = trail.lifeTime;
	}
}

void GPUEmitter::SetEmitWorldPosition(const Vector3& worldPos)
{
	// 継続発生中の追従用。translate のみ差し替え、count/interval/isEmit は UpdateEmitters に委ねる
	switch (currentShape_) {
	case EmitterShape::Sphere:   if (emitterSphereData_)   emitterSphereData_->translate = worldPos;   break;
	case EmitterShape::Box:      if (emitterBoxData_)      emitterBoxData_->translate = worldPos;      break;
	case EmitterShape::Triangle: if (emitterTriangleData_) emitterTriangleData_->translate = worldPos; break;
	case EmitterShape::Cone:     if (emitterConeData_)     emitterConeData_->translate = worldPos;     break;
	case EmitterShape::Mesh:     if (emitterMeshData_)     emitterMeshData_->translate = worldPos;     break;
	}
}

Vector3 GPUEmitter::GetEmitterPosition() const
{
	switch (currentShape_) {
	case EmitterShape::Sphere:   return emitterSphereData_->translate;
	case EmitterShape::Box:      return emitterBoxData_->translate;
	case EmitterShape::Triangle: return emitterTriangleData_->translate;
	case EmitterShape::Cone:     return emitterConeData_->translate;
	case EmitterShape::Mesh:     return emitterMeshData_->translate;
	}
	return { 0,0,0 };
}

void GPUEmitter::SetCamera(Camera* camera)
{
	// 自身のカメラポインタを更新
	camera_ = camera;

	// 内部のGPUParticleのカメラポインタを更新
	// ※ GPUParticle にも SetCamera(Camera*) があることを前提とします。
	if (gpuParticle_) {
		// 
		gpuParticle_->SetCamera(camera);
	}
}
