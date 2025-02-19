#pragma once

// C++
#include <optional>
#include <map>
#include <vector>
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <vector>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <span>

// Engine
#include "Joint.h"
#include "Node.h"
#include "Skeleton.h"
#include "Joint.h"


// Math
#include "Quaternion.h"
#include "Vector3.h"



class SkinCluster
{

public:


	/// <summary>
	/// 更新
	/// </summary>
	void Update(std::vector<Joint> joints_);

	/// <summary>
	/// リソース
	/// </summary>
	void CreateResource(size_t jointsSize,size_t verticesSize, std::map<std::string, int32_t> jointMap);


private:
	struct VertexWeightData {
		float weight;
		uint32_t vertexIndex;
	};
	struct JointWeightData {
		Matrix4x4 inverseBindPoseMatrix;
		std::vector<VertexWeightData> vertexWeights;
	};
	// インフルエンス
	const static uint32_t kNumMaxInfluence = 4;
	struct VertexInfluence {
		std::array<float, kNumMaxInfluence> weights;
		std::array<int32_t, kNumMaxInfluence> jointindices;
	};
	// マトリックスパレット
	struct WellForGPU {
		Matrix4x4 skeletonSpaceMatrix;  // 位置用
		Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 法線用
	};

	std::map<std::string, JointWeightData> skinClusterData_;
	std::vector<Matrix4x4> inverseBindposeMatrices_;
	Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;
	D3D12_VERTEX_BUFFER_VIEW influenceBufferView_;
	std::span<VertexInfluence> mappedInfluence_;
	uint32_t influSRVIndex_;
	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
	std::span<WellForGPU> mappedPalette_;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_;
	uint32_t srvIndex_;

};

