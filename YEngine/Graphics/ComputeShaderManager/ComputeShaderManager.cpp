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

	// Root Parameters
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

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

	// ===== Root Parameters =====
	D3D12_ROOT_PARAMETER rootParameters[13] = {};

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

	// ===== Root Parameters =====
	// UpdateCS は「UAV ×1 + PerFrame(CBV1) + FreeListIndex(UAV) + FreeList(UAV)」
	D3D12_ROOT_PARAMETER rootParameters[6] = {};

	// UAV(Particle)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = particleUAV;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(particleUAV);

	// PerFrame (CBV0)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// ParticleParams (CBV1)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].Descriptor.ShaderRegister = 1;

	// FreeListIndex UAV
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = freeListIndexUAV;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(freeListIndexUAV);

	// FreeList UAV
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].DescriptorTable.pDescriptorRanges = freeListUAV;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(freeListUAV);

	// ActiveCount UAV
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[5].DescriptorTable.pDescriptorRanges = activeCountUAV;
	rootParameters[5].DescriptorTable.NumDescriptorRanges = _countof(activeCountUAV);



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
}
