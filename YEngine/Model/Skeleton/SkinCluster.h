#pragma once

// C++
#include <optional>
#include <map>
#include <vector>
#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <span>

// Engine
#include "Joint.h"
#include "../Node/Node.h"
#include "Skeleton.h"

// Math
#include "Quaternion.h"
#include "Vector4.h"
#include "Vector3.h"
#include "Vector2.h"
#include <Mesh/Mesh.h>

// スキンクラスタークラス
class SkinCluster
{
public:
	///************************* 構造体定義 *************************///

	// 頂点ウェイト情報
	struct VertexWeightData {
		float weight;
		uint32_t vertexIndex;
	};

	// ジョイントウェイト情報
	struct JointWeightData {
		Matrix4x4 inverseBindPoseMatrix;
		std::vector<VertexWeightData> vertexWeights;
	};

	// 最大影響数
	static const uint32_t kNumMaxInfluence = 4u;

	// 頂点影響情報
	struct VertexInfluence {
		std::array<float, 4> weights;
		std::array<int32_t, 4> jointindices;
	};

	// GPU用マトリックス
	struct WellForGPU {
		Matrix4x4 skeletonSpaceMatrix;
		Matrix4x4 skeletonSpaceInverseTransposeMatrix;
	};

	// スキニング情報
	struct SkinningInformation {
		uint32_t numVertices;
	};

	// 頂点データ
	struct Vertex {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// 法線計算用キャッシュ
	struct MatrixCache {
		Matrix4x4 lastMatrix;
		Matrix4x4 inverseTranspose;
		bool valid = false;
	};

public:
	///************************* 基本関数 *************************///

	// マトリクスパレット更新
	void UpdateMatrixPalette(const std::vector<Joint>& joints);

	// コンピュートリソース作成
	void CreateResourceCS(size_t jointsSize, size_t verticesSize, std::map<std::string, int32_t> jointMap);

	// 入力頂点設定
	void SetInputVertices(const std::vector<Vertex>& vertices);

	// シーンから読み込み
	void LoadFromScene(const aiScene* scene);

	// スキニング実行
	void ExecuteSkinningCS();

	// 終了処理
	void Finalize();

	// デストラクタ
	~SkinCluster();

public:
	///************************* アクセッサ *************************///

	// バインドポーズ
	const std::vector<Matrix4x4>& GetInverseBindposeMatrices() const { return inverseBindposeMatrices_; }
	void SetInverseBindposeMatrices(const std::vector<Matrix4x4>& matrices) { inverseBindposeMatrices_ = matrices; }

	// インフルエンス
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetInfluenceResource() const { return influenceResource_; }
	void SetInfluenceResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource) { influenceResource_ = resource; }

	const D3D12_VERTEX_BUFFER_VIEW& GetInfluenceBufferView() const { return influenceBufferView_; }
	void SetInfluenceBufferView(const D3D12_VERTEX_BUFFER_VIEW& view) { influenceBufferView_ = view; }

	std::span<VertexInfluence> GetMappedInfluence() const { return mappedInfluence_; }
	void SetMappedInfluence(std::span<VertexInfluence> influence) { mappedInfluence_ = influence; }

	uint32_t GetInfluSRVIndex() const { return influSRVIndex_; }
	void SetInfluSRVIndex(uint32_t index) { influSRVIndex_ = index; }

	// パレット
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetPaletteResource() const { return paletteResource_; }
	void SetPaletteResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource) { paletteResource_ = resource; }

	const std::span<WellForGPU> GetMappedPalette() { return mappedPalette_; }
	void SetMappedPalette(std::span<WellForGPU> palette) { mappedPalette_ = palette; }

	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> GetPaletteSrvHandle() const { return paletteSrvHandle_; }
	void SetPaletteSrvHandle(std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> handle) { paletteSrvHandle_ = handle; }

	uint32_t GetSRVIndex() const { return srvIndex_; }
	void SetSRVIndex(uint32_t index) { srvIndex_ = index; }

	// 出力
	ID3D12Resource* GetOutputResource() const { return outputResource_.Get(); }
	ID3D12Resource* GetSkinningInformationResource() const { return skinningInformationResource_.Get(); }
	const D3D12_VERTEX_BUFFER_VIEW& GetOutputBufferView() const { return outputBufferView_; }

	// 各メッシュデータ
	void SetSkinClusterDataPerMesh(const std::vector<std::map<std::string, JointWeightData>>& allData) { allMeshJointData_ = allData; }

	// READBACKリソース
	ID3D12Resource* GetReadbackResource() const { return readbackResource_.Get(); }

private:
	///************************* メンバ変数 *************************///

	// バインドポーズ関連
	std::vector<Matrix4x4> inverseBindposeMatrices_;

	// インフルエンス関連
	Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;
	D3D12_VERTEX_BUFFER_VIEW influenceBufferView_;
	std::span<VertexInfluence> mappedInfluence_;
	uint32_t influSRVIndex_;

	// パレット関連
	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
	std::span<WellForGPU> mappedPalette_;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_;
	uint32_t srvIndex_;

	// 出力関連
	Microsoft::WRL::ComPtr<ID3D12Resource> outputResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationResource_;
	SkinningInformation* mappedSkinningInfo_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> inputVerticesResource_;
	Vertex* mappedInputVertices_ = nullptr;
	uint32_t uavIndex_ = 0;
	D3D12_VERTEX_BUFFER_VIEW outputBufferView_{};

	// メッシュデータ
	std::vector<std::map<std::string, JointWeightData>> allMeshJointData_;
	std::vector<size_t> meshVertexOffset_;
	std::vector<size_t> meshVertexCounts_;

	// キャッシュ関連
	std::vector<MatrixCache> matrixCache_;

	// GPUリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource_;

	// パイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
};
