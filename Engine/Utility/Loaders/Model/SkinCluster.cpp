#include "SkinCluster.h"
#include "../Core/DX/DirectXCommon.h"
#include "../Graphics/SrvManager/SrvManager.h"
void SkinCluster::Update(std::vector<Joint> joints_)
{
	for (size_t jointIndex = 0; jointIndex < joints_.size(); ++jointIndex) {
		assert(jointIndex < inverseBindposeMatrices_.size());
		// スケルトン空間行列の計算
		mappedPalette_[jointIndex].skeletonSpaceMatrix = inverseBindposeMatrices_[jointIndex] * joints_[jointIndex].GetSkeletonSpaceMatrix();
		// 法線用の行列を計算（転置逆行列）
		mappedPalette_[jointIndex].skeletonSpaceInverseTransposeMatrix = TransPose(Inverse(mappedPalette_[jointIndex].skeletonSpaceMatrix));
	}
}

void SkinCluster::CreateResource(size_t jointsSize, size_t verticesSize, std::map<std::string, int32_t> jointMap)
{
	//=========================================================//
	//					palette用のResourceを確保				   //
	//=========================================================//
	paletteResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(WellForGPU) * jointsSize);
	WellForGPU* mappedPalette = nullptr;
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	mappedPalette_ = { mappedPalette,jointsSize}; // spanを使ってアクセスできるようにする
	srvIndex_ = SrvManager::GetInstance()->Allocate();
	paletteSrvHandle_.first = SrvManager::GetInstance()->GetCPUSRVDescriptorHandle(srvIndex_);
	paletteSrvHandle_.second = SrvManager::GetInstance()->GetGPUSRVDescriptorHandle(srvIndex_);
	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(srvIndex_, paletteResource_.Get(),
	UINT(jointsSize), sizeof(WellForGPU));

	//=========================================================//
	//					Influece用のResourceを生成			   //
	//=========================================================//
	influenceResource_ = DirectXCommon::GetInstance()->CreateBufferResource(sizeof(VertexInfluence) * verticesSize);
	VertexInfluence* mappedInfluence = nullptr;
	influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * verticesSize); // 0埋め。weightを0にしておく
	mappedInfluence_ = { mappedInfluence,verticesSize };
	/// Influence用のVBVを作成
	influenceBufferView_.BufferLocation = influenceResource_->GetGPUVirtualAddress();
	influenceBufferView_.SizeInBytes = UINT(sizeof(VertexInfluence) *verticesSize);
	influenceBufferView_.StrideInBytes = sizeof(VertexInfluence);
	/// InverseBindPosematrixを格納する場所を作成して、単位行列で埋める
	inverseBindposeMatrices_.resize(jointsSize);
	std::generate(inverseBindposeMatrices_.begin(), inverseBindposeMatrices_.end(), []() { return MakeIdentity4x4(); });

	//=========================================================//
	//			ModelDataを解析してInfluenceを埋める			   //
	//=========================================================//
	for (const auto& jointWeight : skinClusterData_) {	/// ModelのSkinCluster情報を解析
		auto it = jointMap.find(jointWeight.first);		/// jointweight.firstはjoint名なので、skeletonに対象となるjointは含まれるか判断
		if (it == jointMap.end()) {						/// そんな名前は存在しない。なので次に回す
			continue;
		}
		/// (*it).secondにはjointのindexが入ってるので、該当のindexのinverseBindPoseMatrixを代入
		inverseBindposeMatrices_[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
			auto& currentInfluence = mappedInfluence_[vertexWeight.vertexIndex]; /// 該当のいvertexIndexのinfluence情報を参照しておく
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
				if (currentInfluence.weights[index] == 0.0f) {					 /// weight == 0が開いてる状態なので、その場所でweightとjointのindexを代入
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointindices[index] = (*it).second;
					break;
				}
			}
		}

	}
}
