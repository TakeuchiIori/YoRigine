#include "ComputeShaderManager.h"
#include "Debugger/Logger.h"

/// <summary>
/// シングルトンインスタンスを取得
/// </summary>
ComputeShaderManager* ComputeShaderManager::GetInstance()
{
	static ComputeShaderManager instance;
	return &instance;
}

/// <summary>
/// 初期化処理（Compute Shader 用 PSO・RootSignature をすべて生成）
/// </summary>
void ComputeShaderManager::Initialize()
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();

	// 各コンピュートシェーダーの PSO / RootSignature を生成
	CreateSkinningCS();
	CreatePaticleInitCS();
	CreateEmitCS();
	CreateParticleUpdateCS();
	CreateResetDrawArgsCS();
	CreatePostEffectCS();
}

/// <summary>
/// RootSignature の取得
/// </summary>
/// <param name="key">登録時のキー名</param>
ID3D12RootSignature* ComputeShaderManager::GetRootSignature(const std::string& key)
{
	auto it = rootSignatures_.find(key);
	if (it != rootSignatures_.end()) {
		return rootSignatures_[key].Get();
	} else {
		return nullptr;
	}
}

/// <summary>
/// Compute パイプラインステートの取得
/// </summary>
/// <param name="key">登録キー名</param>
ID3D12PipelineState* ComputeShaderManager::GetComputePipelineState(const std::string& key)
{
	auto it = computePipelineStates_.find(key);
	if (it != computePipelineStates_.end()) {
		return computePipelineStates_[key].Get();
	} else {
		return nullptr;
	}
}

/// <summary>
/// 全 Compute PSO と RootSignature の解放
/// </summary>
void ComputeShaderManager::Finalize() {
	// パイプラインステートオブジェクトの解放
	for (auto& pso : computePipelineStates_) {
		pso.second.Reset();
	}
	computePipelineStates_.clear();

	// ルートシグネチャの解放
	for (auto& rs : rootSignatures_) {
		rs.second.Reset();
	}
	rootSignatures_.clear();
}

/// <summary>
/// Skinning（スキニング）用 Compute Shader の RootSignature と PSO を生成
/// </summary>
void ComputeShaderManager::CreateSkinningCS()
{
	HRESULT hr;

	// ===== SRV (t0～t2) =====
	D3D12_DESCRIPTOR_RANGE descriptorRangesSRV[3] = {};
	// t0: gMatrixPalette
	descriptorRangesSRV[0].BaseShaderRegister = 0;
	descriptorRangesSRV[0].NumDescriptors = 1;
	descriptorRangesSRV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangesSRV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// t1: gInputVertices
	descriptorRangesSRV[1].BaseShaderRegister = 1;
	descriptorRangesSRV[1].NumDescriptors = 1;
	descriptorRangesSRV[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangesSRV[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// t2: gInfluences
	descriptorRangesSRV[2].BaseShaderRegister = 2;
	descriptorRangesSRV[2].NumDescriptors = 1;
	descriptorRangesSRV[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangesSRV[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ===== UAV (u0) =====
	D3D12_DESCRIPTOR_RANGE descriptorRangeUAV[1] = {};
	descriptorRangeUAV[0].BaseShaderRegister = 0;
	descriptorRangeUAV[0].NumDescriptors = 1;
	descriptorRangeUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRangeUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// ===== Root Parameters =====
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	// SRV Table
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRangesSRV;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangesSRV);

	// UAV Table
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeUAV;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeUAV);

	// CBV (b0)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 0;

	// ===== RootSignature Desc =====
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;

	// ===== Serialize =====
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Logger(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ===== Create RootSignature =====
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatures_["SkinningCS"])
	);
	assert(SUCCEEDED(hr));

	// ===== Load Shader =====
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob =
		dxCommon_->CompileShader(L"Resources/Shaders/Skinning/Skinning.CS.hlsl", L"cs_6_0");

	// ===== Create PSO =====
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = rootSignatures_["SkinningCS"].Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&computePipelineStates_["SkinningCS"])
	);
}

/// <summary>
/// パーティクル初期化 (InitializeParticle.CS) の PSO / RootSignature を生成
/// </summary>
void ComputeShaderManager::CreatePaticleInitCS()
{
	HRESULT hr;
	// UAV: Particle
	D3D12_DESCRIPTOR_RANGE particleUAV[1] = {};
	particleUAV[0].BaseShaderRegister = 0;
	particleUAV[0].NumDescriptors = 1;
	particleUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	particleUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: FreeListIndex
	D3D12_DESCRIPTOR_RANGE freeListIndexUAV[1] = {};
	freeListIndexUAV[0].BaseShaderRegister = 1;
	freeListIndexUAV[0].NumDescriptors = 1;
	freeListIndexUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	freeListIndexUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: FreeList
	D3D12_DESCRIPTOR_RANGE freeListUAV[1] = {};
	freeListUAV[0].BaseShaderRegister = 2;
	freeListUAV[0].NumDescriptors = 1;
	freeListUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	freeListUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: ActiveCount
	D3D12_DESCRIPTOR_RANGE activeCountUAV[1] = {};
	activeCountUAV[0].BaseShaderRegister = 3;
	activeCountUAV[0].NumDescriptors = 1;
	activeCountUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	activeCountUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: Warm (u4) / Cold (u5) — SoA バッファ
	D3D12_DESCRIPTOR_RANGE warmUAV[1] = {};
	warmUAV[0].BaseShaderRegister = 4;
	warmUAV[0].NumDescriptors = 1;
	warmUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	warmUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE coldUAV[1] = {};
	coldUAV[0].BaseShaderRegister = 5;
	coldUAV[0].NumDescriptors = 1;
	coldUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	coldUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// Root Parameters
	D3D12_ROOT_PARAMETER rootParameters[6] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = particleUAV;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(particleUAV);

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = freeListIndexUAV;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(freeListIndexUAV);

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = freeListUAV;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(freeListUAV);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = activeCountUAV;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(activeCountUAV);

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = warmUAV;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(warmUAV);

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[5].DescriptorTable.pDescriptorRanges = coldUAV;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = _countof(coldUAV);


	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);
	if (FAILED(hr)) {
		Logger(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatures_["ParticleInitCS"])
	);
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"Resources/Shaders/Particle/InitializeParticle.CS.hlsl", L"cs_6_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = rootSignatures_["ParticleInitCS"].Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&computePipelineStates_["ParticleInitCS"])
	);
}

/// <summary>
/// パーティクル Emit（発生）用 Compute Shader の PSO / RootSignature を生成
/// </summary>
void ComputeShaderManager::CreateEmitCS()
{
	HRESULT hr;

	// ===== UAV =====
	D3D12_DESCRIPTOR_RANGE particleUAV[1] = {};
	particleUAV[0].BaseShaderRegister = 0;
	particleUAV[0].NumDescriptors = 1;
	particleUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	particleUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE freeListIndexUAV[1] = {};
	freeListIndexUAV[0].BaseShaderRegister = 1;
	freeListIndexUAV[0].NumDescriptors = 1;
	freeListIndexUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	freeListIndexUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE freeListUAV[1] = {};
	freeListUAV[0].BaseShaderRegister = 2;
	freeListUAV[0].NumDescriptors = 1;
	freeListUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	freeListUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: ActiveCount
	D3D12_DESCRIPTOR_RANGE activeCountUAV[1] = {};
	activeCountUAV[0].BaseShaderRegister = 3;
	activeCountUAV[0].NumDescriptors = 1;
	activeCountUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	activeCountUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE meshTrianglesSRV[1] = {};
	meshTrianglesSRV[0].BaseShaderRegister = 0;
	meshTrianglesSRV[0].NumDescriptors = 1;
	meshTrianglesSRV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	meshTrianglesSRV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	// UAV: Warm (u4) / Cold (u5) — SoA バッファ
	D3D12_DESCRIPTOR_RANGE warmUAV[1] = {};
	warmUAV[0].BaseShaderRegister = 4;
	warmUAV[0].NumDescriptors = 1;
	warmUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	warmUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE coldUAV[1] = {};
	coldUAV[0].BaseShaderRegister = 5;
	coldUAV[0].NumDescriptors = 1;
	coldUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	coldUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// ===== Root Parameters =====
	D3D12_ROOT_PARAMETER rootParameters[17] = {};

	// Emitter Parameters (CBV0～CBV6)
	for (int i = 0; i <= 7; i++) {
		rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[i].Descriptor.ShaderRegister = i;
	}

	// UAV Tables
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[8].DescriptorTable.pDescriptorRanges = particleUAV;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(particleUAV);

	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[9].DescriptorTable.pDescriptorRanges = freeListIndexUAV;
	rootParameters[9].DescriptorTable.NumDescriptorRanges = _countof(freeListIndexUAV);

	rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[10].DescriptorTable.pDescriptorRanges = freeListUAV;
	rootParameters[10].DescriptorTable.NumDescriptorRanges = _countof(freeListUAV);

	rootParameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[11].DescriptorTable.pDescriptorRanges = activeCountUAV;
	rootParameters[11].DescriptorTable.NumDescriptorRanges = _countof(activeCountUAV);

	rootParameters[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[12].DescriptorTable.pDescriptorRanges = meshTrianglesSRV;
	rootParameters[12].DescriptorTable.NumDescriptorRanges = _countof(meshTrianglesSRV);

	rootParameters[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[13].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[13].DescriptorTable.pDescriptorRanges = warmUAV;
	rootParameters[13].DescriptorTable.NumDescriptorRanges = _countof(warmUAV);

	rootParameters[14].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[14].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[14].DescriptorTable.pDescriptorRanges = coldUAV;
	rootParameters[14].DescriptorTable.NumDescriptorRanges = _countof(coldUAV);

	// 15: EmitterRing CBV b8 / 16: EmitterLine CBV b9 (末尾追加 — 既存インデックスを変更しない)
	rootParameters[15].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[15].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[15].Descriptor.ShaderRegister = 8;

	rootParameters[16].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[16].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[16].Descriptor.ShaderRegister = 9;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Logger(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// RootSignature 作成
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatures_["EmitCS"])
	);
	assert(SUCCEEDED(hr));

	// Shader 読み込み
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob =
		dxCommon_->CompileShader(L"Resources/Shaders/Particle/EmitParticle.CS.hlsl", L"cs_6_0");

	// PSO 作成
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = rootSignatures_["EmitCS"].Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&computePipelineStates_["EmitCS"])
	);
}

/// <summary>
/// パーティクル更新 (UpdateParticle.CS) の PSO / RootSignature を生成
/// </summary>
void ComputeShaderManager::CreateParticleUpdateCS()
{
	HRESULT hr;

	// ===== UAV =====
	D3D12_DESCRIPTOR_RANGE particleUAV[1] = {};
	particleUAV[0].BaseShaderRegister = 0;
	particleUAV[0].NumDescriptors = 1;
	particleUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	particleUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE freeListIndexUAV[1] = {};
	freeListIndexUAV[0].BaseShaderRegister = 1;
	freeListIndexUAV[0].NumDescriptors = 1;
	freeListIndexUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	freeListIndexUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE freeListUAV[1] = {};
	freeListUAV[0].BaseShaderRegister = 2;
	freeListUAV[0].NumDescriptors = 1;
	freeListUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	freeListUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: ActiveCount
	D3D12_DESCRIPTOR_RANGE activeCountUAV[1] = {};
	activeCountUAV[0].BaseShaderRegister = 3;
	activeCountUAV[0].NumDescriptors = 1;
	activeCountUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	activeCountUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// SRV: ForceFields (t0) — GPU フォースフィールド配列。0個時もバッファは存在し HLSL 側でカウント0ガード
	D3D12_DESCRIPTOR_RANGE forceFieldSRV[1] = {};
	forceFieldSRV[0].BaseShaderRegister = 0;
	forceFieldSRV[0].NumDescriptors = 1;
	forceFieldSRV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	forceFieldSRV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	// UAV: Warm (u4) / Cold (u5) — Update 本体は未使用。共用する SpawnTrailCS が読み書きする
	D3D12_DESCRIPTOR_RANGE warmUAV[1] = {};
	warmUAV[0].BaseShaderRegister = 4;
	warmUAV[0].NumDescriptors = 1;
	warmUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	warmUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE coldUAV[1] = {};
	coldUAV[0].BaseShaderRegister = 5;
	coldUAV[0].NumDescriptors = 1;
	coldUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	coldUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// UAV: DrawList (u6) / DrawArgs (u7) — インダイレクト描画。Update CS が生存粒子を追記する
	D3D12_DESCRIPTOR_RANGE drawListUAV[1] = {};
	drawListUAV[0].BaseShaderRegister = 6;
	drawListUAV[0].NumDescriptors = 1;
	drawListUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	drawListUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_DESCRIPTOR_RANGE drawArgsUAV[1] = {};
	drawArgsUAV[0].BaseShaderRegister = 7;
	drawArgsUAV[0].NumDescriptors = 1;
	drawArgsUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	drawArgsUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	// SRV: NoiseFields (t1) — Curl/Turbulence/Vortex ノイズ配列。0個時もバッファは存在し HLSL 側でカウント0ガード
	D3D12_DESCRIPTOR_RANGE noiseFieldSRV[1] = {};
	noiseFieldSRV[0].BaseShaderRegister = 1;
	noiseFieldSRV[0].NumDescriptors = 1;
	noiseFieldSRV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	noiseFieldSRV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	// SRV: AccelerationFields (t2) — 範囲内一定加速度の配列。0個時もバッファは存在し HLSL 側でカウント0ガード
	D3D12_DESCRIPTOR_RANGE accelerationFieldSRV[1] = {};
	accelerationFieldSRV[0].BaseShaderRegister = 2;
	accelerationFieldSRV[0].NumDescriptors = 1;
	accelerationFieldSRV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	accelerationFieldSRV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	// ===== Root Parameters =====
	D3D12_ROOT_PARAMETER rootParameters[14] = {};

	// 0: UAV(Particle) u0
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = particleUAV;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(particleUAV);

	// 1: PerFrame CBV b0
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// 2: ParticleParams CBV b1
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 1;

	// 3: FreeListIndex UAV u1
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = freeListIndexUAV;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(freeListIndexUAV);

	// 4: FreeList UAV u2
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = freeListUAV;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(freeListUAV);

	// 5: ActiveCount UAV u3
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[5].DescriptorTable.pDescriptorRanges = activeCountUAV;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = _countof(activeCountUAV);

	// 6: ForceFields SRV t0 (末尾追加 — 既存インデックスを変更しない)
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[6].DescriptorTable.pDescriptorRanges = forceFieldSRV;
	rootParameters[6].DescriptorTable.NumDescriptorRanges = _countof(forceFieldSRV);

	// 7: Warm UAV u4 / 8: Cold UAV u5 (SpawnTrailCS 用。Update 本体は未使用)
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[7].DescriptorTable.pDescriptorRanges = warmUAV;
	rootParameters[7].DescriptorTable.NumDescriptorRanges = _countof(warmUAV);

	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[8].DescriptorTable.pDescriptorRanges = coldUAV;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(coldUAV);

	// 9: DrawList UAV u6 / 10: DrawArgs UAV u7 (Update CS が生存粒子を追記)
	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[9].DescriptorTable.pDescriptorRanges = drawListUAV;
	rootParameters[9].DescriptorTable.NumDescriptorRanges = _countof(drawListUAV);

	rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[10].DescriptorTable.pDescriptorRanges = drawArgsUAV;
	rootParameters[10].DescriptorTable.NumDescriptorRanges = _countof(drawArgsUAV);

	// 11: NoiseFields SRV t1 (末尾追加 — 既存インデックスを変更しない)
	rootParameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[11].DescriptorTable.pDescriptorRanges = noiseFieldSRV;
	rootParameters[11].DescriptorTable.NumDescriptorRanges = _countof(noiseFieldSRV);

	// 12: AccelerationFields SRV t2 (末尾追加 — 既存インデックスを変更しない)
	rootParameters[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[12].DescriptorTable.pDescriptorRanges = accelerationFieldSRV;
	rootParameters[12].DescriptorTable.NumDescriptorRanges = _countof(accelerationFieldSRV);

	// 13: ExtParams CBV b2 (Drag/Bounce。VS 側は b1 で同じ内容を読む)
	rootParameters[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[13].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[13].Descriptor.ShaderRegister = 2;



	// RootSignature Desc
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;


	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);
	if (FAILED(hr)) {
		Logger(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatures_["ParticleUpdateCS"])
	);
	assert(SUCCEEDED(hr));

	// Shader
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob =
		dxCommon_->CompileShader(L"Resources/Shaders/Particle/UpdateParticle.CS.hlsl", L"cs_6_0");

	// PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = rootSignatures_["ParticleUpdateCS"].Get();
	computePipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc,
		IID_PPV_ARGS(&computePipelineStates_["ParticleUpdateCS"])
	);

	// トレイル生成パス（FreeList Pop 専用。Update パスの Push と分離して競合を防ぐ）
	// ルートシグネチャは ParticleUpdateCS と共用
	Microsoft::WRL::ComPtr<IDxcBlob> trailSpawnBlob =
		dxCommon_->CompileShader(L"Resources/Shaders/Particle/SpawnTrailParticle.CS.hlsl", L"cs_6_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC trailSpawnDesc = {};
	trailSpawnDesc.pRootSignature = rootSignatures_["ParticleUpdateCS"].Get();
	trailSpawnDesc.CS = { trailSpawnBlob->GetBufferPointer(), trailSpawnBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&trailSpawnDesc,
		IID_PPV_ARGS(&computePipelineStates_["TrailSpawnCS"])
	);
}

// =====================================================================
// インダイレクト描画の引数バッファ（D3D12_DRAW_INDEXED_ARGUMENTS）を毎フレーム初期化する
// 1スレッドの CS。u0=DrawArgs(RWStructuredBuffer<uint>×5)、b0=索引数(root定数)。
// =====================================================================
void ComputeShaderManager::CreateResetDrawArgsCS()
{
	HRESULT hr;

	// UAV: DrawArgs (u0)
	D3D12_DESCRIPTOR_RANGE drawArgsUAV[1] = {};
	drawArgsUAV[0].BaseShaderRegister = 0;
	drawArgsUAV[0].NumDescriptors = 1;
	drawArgsUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	drawArgsUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	// 0: DrawArgs UAV u0
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = drawArgsUAV;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(drawArgsUAV);
	// 1: 索引数 root 定数 (b0)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Constants.ShaderRegister = 0;
	rootParameters[1].Constants.RegisterSpace = 0;
	rootParameters[1].Constants.Num32BitValues = 1;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Logger(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatures_["ResetDrawArgsCS"]));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlob> csBlob =
		dxCommon_->CompileShader(L"Resources/Shaders/Particle/ResetDrawArgs.CS.hlsl", L"cs_6_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignatures_["ResetDrawArgsCS"].Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&psoDesc, IID_PPV_ARGS(&computePipelineStates_["ResetDrawArgsCS"]));
	assert(SUCCEEDED(hr));
}

// =====================================================================
// PostEffect CS の RootSignature + PSO 一括生成
// 5種類の共通RS と 19個のPSO を作成する。OffScreen::RenderEffectCompute から参照される。
// =====================================================================
namespace {

	// 1個分の descriptor range を作る
	D3D12_DESCRIPTOR_RANGE MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT baseReg, UINT count = 1)
	{
		D3D12_DESCRIPTOR_RANGE r{};
		r.RangeType = type;
		r.BaseShaderRegister = baseReg;
		r.NumDescriptors = count;
		r.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		return r;
	}

	// 線形サンプラ
	D3D12_STATIC_SAMPLER_DESC MakeStaticSampler(UINT shaderReg, D3D12_FILTER filter)
	{
		D3D12_STATIC_SAMPLER_DESC s{};
		s.Filter = filter;
		s.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		s.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		s.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		s.MaxLOD = D3D12_FLOAT32_MAX;
		s.ShaderRegister = shaderReg;
		s.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		return s;
	}

	void SerializeAndCreateRS(
		ID3D12Device* device,
		const D3D12_ROOT_SIGNATURE_DESC& desc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>& outRS)
	{
		Microsoft::WRL::ComPtr<ID3DBlob> sig;
		Microsoft::WRL::ComPtr<ID3DBlob> err;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
		if (FAILED(hr)) {
			Logger(reinterpret_cast<char*>(err->GetBufferPointer()));
			assert(false);
		}
		hr = device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&outRS));
		assert(SUCCEEDED(hr));
		(void)hr;
	}
}

void ComputeShaderManager::CreatePostEffectCS()
{
	auto device = dxCommon_->GetDevice().Get();

	// ===========================================================
	// RS_Simple : SRV(t0) + UAV(u0) + Sampler(s0)
	// Copy/Sepia/Grayscale/Vignette 用
	// ===========================================================
	{
		D3D12_DESCRIPTOR_RANGE srv = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		D3D12_DESCRIPTOR_RANGE uav = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);

		D3D12_ROOT_PARAMETER params[2] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[0].DescriptorTable.pDescriptorRanges = &srv;
		params[0].DescriptorTable.NumDescriptorRanges = 1;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].DescriptorTable.pDescriptorRanges = &uav;
		params[1].DescriptorTable.NumDescriptorRanges = 1;

		D3D12_STATIC_SAMPLER_DESC samps[1] = { MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR) };

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = _countof(samps);
		desc.pStaticSamplers = samps;
		SerializeAndCreateRS(device, desc, rootSignatures_["PostEffectRS_Simple"]);
	}

	// ===========================================================
	// RS_CB : SRV(t0) + UAV(u0) + CBV(b0) + Sampler(s0)
	// Gauss/Box/Radial/Tone/Chromatic/Bloom/Posterize/Kuwahara/Halftone/CrossHatch/ColorGrade 用
	// ===========================================================
	{
		D3D12_DESCRIPTOR_RANGE srv = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		D3D12_DESCRIPTOR_RANGE uav = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);

		D3D12_ROOT_PARAMETER params[3] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[0].DescriptorTable.pDescriptorRanges = &srv;
		params[0].DescriptorTable.NumDescriptorRanges = 1;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].DescriptorTable.pDescriptorRanges = &uav;
		params[1].DescriptorTable.NumDescriptorRanges = 1;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].Descriptor.ShaderRegister = 0;

		D3D12_STATIC_SAMPLER_DESC samps[1] = { MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR) };

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = _countof(samps);
		desc.pStaticSamplers = samps;
		SerializeAndCreateRS(device, desc, rootSignatures_["PostEffectRS_CB"]);
	}

	// ===========================================================
	// RS_CB2 : SRV(t0) + UAV(u0) + CBV(b0) + CBV(b1) + Sampler(s0)
	// ColorAdjust 用
	// ===========================================================
	{
		D3D12_DESCRIPTOR_RANGE srv = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		D3D12_DESCRIPTOR_RANGE uav = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);

		D3D12_ROOT_PARAMETER params[4] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[0].DescriptorTable.pDescriptorRanges = &srv;
		params[0].DescriptorTable.NumDescriptorRanges = 1;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].DescriptorTable.pDescriptorRanges = &uav;
		params[1].DescriptorTable.NumDescriptorRanges = 1;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].Descriptor.ShaderRegister = 0;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[3].Descriptor.ShaderRegister = 1;

		D3D12_STATIC_SAMPLER_DESC samps[1] = { MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR) };

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = _countof(samps);
		desc.pStaticSamplers = samps;
		SerializeAndCreateRS(device, desc, rootSignatures_["PostEffectRS_CB2"]);
	}

	// ===========================================================
	// RS_Depth : SRV(t0)+SRV(t1=depth) + UAV(u0) + CBV(b0) + Sampler(s0,s1)
	// DepthOutline / Fog / GodRays 用
	// ===========================================================
	{
		D3D12_DESCRIPTOR_RANGE srv0 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		D3D12_DESCRIPTOR_RANGE srv1 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1);
		D3D12_DESCRIPTOR_RANGE uav  = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);

		D3D12_ROOT_PARAMETER params[4] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[0].DescriptorTable.pDescriptorRanges = &srv0;
		params[0].DescriptorTable.NumDescriptorRanges = 1;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].DescriptorTable.pDescriptorRanges = &srv1;
		params[1].DescriptorTable.NumDescriptorRanges = 1;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].DescriptorTable.pDescriptorRanges = &uav;
		params[2].DescriptorTable.NumDescriptorRanges = 1;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[3].Descriptor.ShaderRegister = 0;

		D3D12_STATIC_SAMPLER_DESC samps[2] = {
			MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR),
			MakeStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_POINT),
		};

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = _countof(samps);
		desc.pStaticSamplers = samps;
		SerializeAndCreateRS(device, desc, rootSignatures_["PostEffectRS_Depth"]);
	}

	// ===========================================================
	// RS_DepthNormal : SRV(t0=color)+SRV(t1=depth)+SRV(t2=normal) + UAV(u0) + CBV(b0) + Sampler(s0,s1)
	// DepthOutline(強化版) / NormalVisualize 用
	// ===========================================================
	{
		D3D12_DESCRIPTOR_RANGE srv0 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		D3D12_DESCRIPTOR_RANGE srv1 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1);
		D3D12_DESCRIPTOR_RANGE srv2 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2);
		D3D12_DESCRIPTOR_RANGE uav  = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);

		D3D12_ROOT_PARAMETER params[5] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[0].DescriptorTable.pDescriptorRanges = &srv0;
		params[0].DescriptorTable.NumDescriptorRanges = 1;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].DescriptorTable.pDescriptorRanges = &srv1;
		params[1].DescriptorTable.NumDescriptorRanges = 1;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].DescriptorTable.pDescriptorRanges = &srv2;
		params[2].DescriptorTable.NumDescriptorRanges = 1;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[3].DescriptorTable.pDescriptorRanges = &uav;
		params[3].DescriptorTable.NumDescriptorRanges = 1;

		params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[4].Descriptor.ShaderRegister = 0;

		D3D12_STATIC_SAMPLER_DESC samps[2] = {
			MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR),
			MakeStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_POINT),
		};

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = _countof(samps);
		desc.pStaticSamplers = samps;
		SerializeAndCreateRS(device, desc, rootSignatures_["PostEffectRS_DepthNormal"]);
	}

	// ===========================================================
	// RS_Tex : SRV(t0)+SRV(t1) + UAV(u0) + CBV(b0) + Sampler(s0)
	// Dissolve / ShatterTransition 用
	// ===========================================================
	{
		D3D12_DESCRIPTOR_RANGE srv0 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		D3D12_DESCRIPTOR_RANGE srv1 = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1);
		D3D12_DESCRIPTOR_RANGE uav  = MakeRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);

		D3D12_ROOT_PARAMETER params[4] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[0].DescriptorTable.pDescriptorRanges = &srv0;
		params[0].DescriptorTable.NumDescriptorRanges = 1;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].DescriptorTable.pDescriptorRanges = &srv1;
		params[1].DescriptorTable.NumDescriptorRanges = 1;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].DescriptorTable.pDescriptorRanges = &uav;
		params[2].DescriptorTable.NumDescriptorRanges = 1;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[3].Descriptor.ShaderRegister = 0;

		D3D12_STATIC_SAMPLER_DESC samps[1] = { MakeStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR) };

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = _countof(samps);
		desc.pStaticSamplers = samps;
		SerializeAndCreateRS(device, desc, rootSignatures_["PostEffectRS_Tex"]);
	}

	// ===========================================================
	// 各エフェクト PSO の作成 (PSOキー, シェーダーパス, RSキー)
	// ===========================================================
	struct PsoSpec { const char* key; const wchar_t* path; const char* rsKey; };
	const PsoSpec specs[] = {
		// Simple
		{ "PostEffectCopyCS",      L"Resources/Shaders/PostEffect/CopyImage/CopyImage.CS.hlsl",        "PostEffectRS_Simple" },
		{ "PostEffectSepiaCS",     L"Resources/Shaders/PostEffect/Sepia/Sepia.CS.hlsl",                "PostEffectRS_Simple" },
		{ "PostEffectGrayscaleCS", L"Resources/Shaders/PostEffect/Grayscale/Grayscale.CS.hlsl",        "PostEffectRS_Simple" },
		{ "PostEffectVignetteCS",  L"Resources/Shaders/PostEffect/Vignette/Vignette.CS.hlsl",          "PostEffectRS_Simple" },
		// CB
		{ "PostEffectGaussCS",       L"Resources/Shaders/PostEffect/Smoothing/GaussianFilter.CS.hlsl",   "PostEffectRS_CB" },
		{ "PostEffectBoxFilterCS",   L"Resources/Shaders/PostEffect/Smoothing/BoxFilter.CS.hlsl",        "PostEffectRS_CB" },
		{ "PostEffectRadialBlurCS",  L"Resources/Shaders/PostEffect/Blur/RadialBlur.CS.hlsl",            "PostEffectRS_CB" },
		{ "PostEffectToneMapCS",     L"Resources/Shaders/PostEffect/ColorRemapping/ToneMapping.CS.hlsl", "PostEffectRS_CB" },
		{ "PostEffectChromaticCS",   L"Resources/Shaders/PostEffect/ColorRemapping/Chromatic.CS.hlsl",   "PostEffectRS_CB" },
		// Dual Kawase ブルーム（ミップピラミッド 3 段構成）
		{ "PostEffectBloomDownCS",   L"Resources/Shaders/PostEffect/Bloom/BloomDownsample.CS.hlsl",      "PostEffectRS_CB" },
		{ "PostEffectBloomUpCS",     L"Resources/Shaders/PostEffect/Bloom/BloomUpsample.CS.hlsl",        "PostEffectRS_CB" },
		{ "PostEffectBloomCompCS",   L"Resources/Shaders/PostEffect/Bloom/BloomComposite.CS.hlsl",       "PostEffectRS_Tex" },
		{ "PostEffectPosterizeCS",   L"Resources/Shaders/PostEffect/Posterize/Posterize.CS.hlsl",        "PostEffectRS_CB" },
		{ "PostEffectKuwaharaCS",    L"Resources/Shaders/PostEffect/Kuwahara/Kuwahara.CS.hlsl",          "PostEffectRS_CB" },
		{ "PostEffectHalftoneCS",    L"Resources/Shaders/PostEffect/Halftone/Halftone.CS.hlsl",          "PostEffectRS_CB" },
		{ "PostEffectCrossHatchCS",  L"Resources/Shaders/PostEffect/CrossHatch/CrossHatch.CS.hlsl",      "PostEffectRS_CB" },
		{ "PostEffectColorGradeCS",  L"Resources/Shaders/PostEffect/ColorGrade/ColorGrade.CS.hlsl",      "PostEffectRS_CB" },
		// CB2
		{ "PostEffectColorAdjustCS", L"Resources/Shaders/PostEffect/ColorRemapping/ColorAdjust.CS.hlsl", "PostEffectRS_CB2" },
		// Depth
		{ "PostEffectDepthOutlineCS", L"Resources/Shaders/PostEffect/OutLine/DepthBasedOutLine.CS.hlsl", "PostEffectRS_DepthNormal" },
		{ "PostEffectNormalVisualizeCS", L"Resources/Shaders/PostEffect/NormalVisualize/NormalVisualize.CS.hlsl", "PostEffectRS_DepthNormal" },
		{ "PostEffectFogCS",          L"Resources/Shaders/PostEffect/Fog/Fog.CS.hlsl",                   "PostEffectRS_Depth" },
		{ "PostEffectGodRaysCS",      L"Resources/Shaders/PostEffect/GodRays/GodRays.CS.hlsl",           "PostEffectRS_Depth" },
		// Tex
		{ "PostEffectDissolveCS",  L"Resources/Shaders/PostEffect/Dissolve/Dissolve.CS.hlsl",                 "PostEffectRS_Tex" },
		{ "PostEffectShatterCS",   L"Resources/Shaders/PostEffect/Transition/ShatterTransition.CS.hlsl",     "PostEffectRS_Tex" },
	};

	for (const auto& s : specs) {
		Microsoft::WRL::ComPtr<IDxcBlob> blob = dxCommon_->CompileShader(s.path, L"cs_6_0");
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = rootSignatures_[s.rsKey].Get();
		psoDesc.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };

		HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computePipelineStates_[s.key]));
		assert(SUCCEEDED(hr));
		(void)hr;
	}
}
