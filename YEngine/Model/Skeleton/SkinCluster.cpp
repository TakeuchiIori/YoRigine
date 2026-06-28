#include "SkinCluster.h"

// Engine
#include "DirectXCommon.h"
#include "../Graphics/ComputeShaderManager/ComputeShaderManager.h"
#include "SrvManager.h"
#include "../ModelUtils.h"
#include "Debugger/Logger.h"


void SkinCluster::UpdateMatrixPalette(const std::vector<Joint>& joints) {
	if (matrixCache_.size() != joints.size()) {
		matrixCache_.resize(joints.size());
	}


	for (size_t jointIndex = 0; jointIndex < joints.size(); ++jointIndex) {

		const Matrix4x4& current = joints[jointIndex].GetSkeletonSpaceMatrix();
		Matrix4x4 finalMatrix = inverseBindposeMatrices_[jointIndex] * current;
		mappedPalette_[jointIndex].skeletonSpaceMatrix = finalMatrix;

		// キャッシュが有効で、行列が変わっていなければ再利用
		if (matrixCache_[jointIndex].valid && matrixCache_[jointIndex].lastMatrix == finalMatrix) {
			mappedPalette_[jointIndex].skeletonSpaceInverseTransposeMatrix = matrixCache_[jointIndex].inverseTranspose;
		} else {
			// 計算＆キャッシュ保存
			Matrix4x4 inverseT = TransPose(Inverse(finalMatrix));
			mappedPalette_[jointIndex].skeletonSpaceInverseTransposeMatrix = inverseT;
			matrixCache_[jointIndex].lastMatrix = finalMatrix;
			matrixCache_[jointIndex].inverseTranspose = inverseT;
			matrixCache_[jointIndex].valid = true;
		}

	}
}


void SkinCluster::CreateResourceCS(size_t jointsSize, size_t verticesSize, std::map<std::string, int32_t> jointMap)
{
	// === SRV を4連続で登録（t0〜t2, u0） ===
	UINT baseIndex = SrvManager::GetInstance()->Allocate(4);

	// === Palette用のResource (t0) ===
	paletteResource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(WellForGPU) * jointsSize);
	WellForGPU* mappedPalette = nullptr;
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	mappedPalette_ = { mappedPalette, jointsSize };
	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(baseIndex + 0, paletteResource_.Get(), UINT(jointsSize), sizeof(WellForGPU));
	paletteSrvHandle_.first = SrvManager::GetInstance()->GetCPUDescriptorHandle(baseIndex + 0);
	paletteSrvHandle_.second = SrvManager::GetInstance()->GetGPUDescriptorHandle(baseIndex + 0);

	// === InputVertices用のResource (t1) ===
	inputVerticesResource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(Vertex) * verticesSize);
	inputVerticesResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInputVertices_));
	std::memset(mappedInputVertices_, 0, sizeof(Vertex) * verticesSize);
	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(baseIndex + 1, inputVerticesResource_.Get(), UINT(verticesSize), sizeof(Vertex));

	// === Influence用のResource (t2) ===
	influenceResource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexInfluence) * verticesSize);
	VertexInfluence* mappedInfluence = nullptr;
	influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * verticesSize);
	mappedInfluence_ = { mappedInfluence, verticesSize };
	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(baseIndex + 2, influenceResource_.Get(), UINT(verticesSize), sizeof(VertexInfluence));

	influenceBufferView_.BufferLocation = influenceResource_->GetGPUVirtualAddress();
	influenceBufferView_.SizeInBytes = UINT(sizeof(VertexInfluence) * verticesSize);
	influenceBufferView_.StrideInBytes = sizeof(VertexInfluence);

	srvIndex_ = baseIndex;
	// === OutputVertices（UAV: u0） ===
	outputResource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResourceUAV(sizeof(Vertex) * verticesSize);
	SrvManager::GetInstance()->CreateUAVForStructuredBuffer(baseIndex + 3, outputResource_.Get(), static_cast<UINT>(verticesSize), sizeof(Vertex));
	uavIndex_ = baseIndex + 3;

	outputBufferView_.BufferLocation = outputResource_->GetGPUVirtualAddress();
	outputBufferView_.SizeInBytes = UINT(sizeof(Vertex) * verticesSize);
	outputBufferView_.StrideInBytes = sizeof(Vertex);

	// === CBV（b0） ===
	skinningInformationResource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(SkinningInformation));
	skinningInformationResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedSkinningInfo_));
	mappedSkinningInfo_->numVertices = static_cast<uint32_t>(verticesSize);

	// === BindPose初期化 ===
	inverseBindposeMatrices_.resize(jointsSize);
	std::generate(inverseBindposeMatrices_.begin(), inverseBindposeMatrices_.end(), []() { return MakeIdentity4x4(); });


	// 安全なスキンウェイト構築
	std::unordered_map<size_t, std::vector<std::pair<float, int>>> tempInfluences;
	size_t vertexOffset = 0;

	for (size_t meshIndex = 0; meshIndex < allMeshJointData_.size(); ++meshIndex) {
		const auto& meshData = allMeshJointData_[meshIndex];
		const size_t meshVertexCount = meshVertexCounts_[meshIndex];

		for (const auto& jointWeight : meshData) {
			auto it = jointMap.find(jointWeight.first);
			if (it == jointMap.end()) continue;

			inverseBindposeMatrices_[it->second] = jointWeight.second.inverseBindPoseMatrix;

			for (const auto& vw : jointWeight.second.vertexWeights) {
				size_t globalIndex = vw.vertexIndex + vertexOffset;
				tempInfluences[globalIndex].emplace_back(vw.weight, it->second);
			}
		}
		vertexOffset += meshVertexCount;
	}

	// 正規化＋4つまでに絞って書き込み
	for (const auto& [vertexIndex, influences] : tempInfluences) {
		auto& dst = mappedInfluence_[vertexIndex];

		auto sorted = influences;
		std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
			return a.first > b.first; // 大きい順
			});

		float totalWeight = 0.0f;
		size_t actualCount = std::min<size_t>(SkinCluster::kNumMaxInfluence, sorted.size());
		for (size_t i = 0; i < actualCount; ++i) {
			totalWeight += sorted[i].first;
		}

		if (totalWeight <= 0.0001f) {
			dst.weights[0] = 1.0f;
			dst.jointindices[0] = 0; // safety fallback
			continue;
		}

		for (size_t i = 0; i < actualCount; ++i) {
			dst.weights[i] = sorted[i].first / totalWeight;
			dst.jointindices[i] = sorted[i].second;
		}
	}


	rootSignature_ = ComputeShaderManager::GetInstance()->GetRootSignature("SkinningCS");
	graphicsPipelineState_ = ComputeShaderManager::GetInstance()->GetComputePipelineState("SkinningCS");
}



void SkinCluster::SetInputVertices(const std::vector<Vertex>& vertices) {
	assert(mappedInputVertices_);
	assert(vertices.size() <= mappedInfluence_.size());
	std::memcpy(mappedInputVertices_, vertices.data(), sizeof(Vertex) * vertices.size());
}


/// <summary>
/// 複数メッシュごとのスキンクラスターデータを読み込む
/// </summary>
void SkinCluster::LoadFromScene(const aiScene* scene) {
	std::vector<std::map<std::string, JointWeightData>> allMeshJointData;
	meshVertexCounts_.clear();

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		meshVertexCounts_.push_back(mesh->mNumVertices);

		std::map<std::string, JointWeightData> jointDataPerMesh;

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = jointDataPerMesh[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

			Matrix4x4 bindposeMatrix = MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotate.x, -rotate.y, -rotate.z, rotate.w },
				{ -translate.x, translate.y, translate.z });

			jointWeightData.inverseBindPoseMatrix = Inverse(bindposeMatrix);

			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({
					bone->mWeights[weightIndex].mWeight,
					bone->mWeights[weightIndex].mVertexId
					});
			}
		}

		allMeshJointData.push_back(std::move(jointDataPerMesh));
	}

	SetSkinClusterDataPerMesh(allMeshJointData);
}



void SkinCluster::ExecuteSkinningCS()
{
	auto commandList = YoRigine::DirectXCommon::GetInstance()->GetCommandList();

	// パイプラインとルートシグネチャをセット
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// ディスクリプタヒープセット
	ID3D12DescriptorHeap* descriptorHeaps[] = { SrvManager::GetInstance()->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// SRVディスクリプタテーブル（MatrixPalette, InputVertices, Influences）
	commandList->SetComputeRootDescriptorTable(0, SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_));

	// UAVディスクリプタテーブル（OutputVertices）
	commandList->SetComputeRootDescriptorTable(1, SrvManager::GetInstance()->GetGPUDescriptorHandle(uavIndex_));

	// CBV（SkinningInformation）
	commandList->SetComputeRootConstantBufferView(2, skinningInformationResource_->GetGPUVirtualAddress());

	// Dispatch（頂点数÷スレッドグループサイズ）
	uint32_t threadGroupCount = (mappedSkinningInfo_->numVertices + 1023) / 1024;
	commandList->Dispatch(threadGroupCount, 1, 1);

}

SkinCluster::~SkinCluster()
{
	if (paletteResource_) {
		paletteResource_->Unmap(0, nullptr);
		paletteResource_.Reset();
	}

	if (inputVerticesResource_) {
		inputVerticesResource_->Unmap(0, nullptr);
		inputVerticesResource_.Reset();
		mappedInputVertices_ = nullptr;
	}

	if (influenceResource_) {
		influenceResource_->Unmap(0, nullptr);
		influenceResource_.Reset();
		mappedInfluence_ = {};
	}

	if (outputResource_) {
		outputResource_.Reset();
	}

	if (skinningInformationResource_) {
		skinningInformationResource_->Unmap(0, nullptr);
		skinningInformationResource_.Reset();
		mappedSkinningInfo_ = nullptr;
	}

	if (readbackResource_) {
		readbackResource_.Reset();
	}

	rootSignature_.Reset();
	graphicsPipelineState_.Reset();
}

