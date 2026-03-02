#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <vector>


// Math
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"


// Engine
#include "../Skeleton/SkinCluster.h"



namespace YoRigine {
	class DirectXCommon;
}


class SkinCluster;

// メッシュクラス
class Mesh {
public:
	///************************* GPU用の構造体 *************************///
	// 頂点データ構造
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// GPU用リソース
	struct MeshResource {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	};

	///************************* CPU用の構造体 *************************///

	// メッシュデータ
	struct MeshData {
		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		uint32_t materialIndex = 0;
		uint32_t vertexOffset = 0;
	};

public:
	///*************************基本関数 *************************///

	// 初期化
	void Initialize();
	void Initialize(const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);

	// コマンドリストを積む
	void RecordDrawCommands(ID3D12GraphicsCommandList* command);
	void RecordDrawCommands(ID3D12GraphicsCommandList* command, D3D12_VERTEX_BUFFER_VIEW vbv);
	void RecordDrawCommands(ID3D12GraphicsCommandList* command, SkinCluster& skinCluster);

	// GPUへ転送
	void TransferData();
private:
	///************************* 内部処理 *************************///

	// リソース生成
	void InitResources();

public:
	///************************* アクセッサ *************************///
	// メッシュ関連
	MeshData& GetMeshData() { return meshData_; }
	const MeshData& GetMeshData() const { return meshData_; }
	const MeshResource& GetMeshResource() const { return meshResources_; }

	// 頂点とインデックス
	VertexData* GetVertexData() const { return vertexData_; }
	uint32_t* GetIndexData() const { return indexData_; }
	uint32_t GetVertexCount() const { return static_cast<uint32_t>(meshData_.vertices.size()); }
	uint32_t GetIndexCount() const { return static_cast<uint32_t>(meshData_.indices.size()); }

	// マテリアル関連
	uint32_t GetMaterialIndex() const { return meshData_.materialIndex; }
	void SetMaterialIndex(uint32_t index) { meshData_.materialIndex = index; }

	// ワールド行列
	void SetWorldMatrix(const Matrix4x4& mtx) { worldMatrix_ = mtx; }
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

	// バッファビュー
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return meshResources_.vertexBufferView; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return meshResources_.indexBufferView; }

	// ボーン関連
	void SetHasBones(bool hasBones) { hasBones_ = hasBones; }
	bool HasBones() const { return hasBones_; }

private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_ = nullptr;

	// 各データ
	MeshData meshData_;
	MeshResource meshResources_;
	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	Matrix4x4 worldMatrix_;

	// ボーン関連
	bool hasBones_ = false;
};
