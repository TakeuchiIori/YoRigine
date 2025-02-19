#pragma once
// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <unordered_map>

// Math
#include "MathFunc.h"
#include "Quaternion.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

class DirectXCommon;
class Mesh
{
public:
	/*=================================================================

								構造体

	=================================================================*/
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// メッシュデータ構造体
	struct MeshData {
		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		uint32_t materialIndex;
	};

	// GPU用リソース構造体
	struct MeshResource {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		uint32_t vertexCount;
		uint32_t indexCount;
	};

public:
	/*=================================================================

							メンバ関数

	=================================================================*/

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// リソース初期化
	/// </summary>
	void InitializeResource();

	/// <summary>
	/// リソース作成
	/// </summary>
	void CreateResources();

	/// <summary>
	/// デフォルトの頂点リソース
	/// </summary>
	void CreateObjVertexResource();

	/// <summary>
	/// デフォルトのインデックスリソース
	/// </summary>
	void CreateObjIndexResource();

	/// <summary>
	/// デフォルトの頂点バッファ
	/// </summary>
	void CreateObjVertexBufferView();

	/// <summary>
	/// デフォルトのインデックバッファ
	/// </summary>
	void CreateObjIndexBufferView();

	/// <summary>
	/// データの転送
	/// </summary>
	void TransferData();

	/// <summary>
	/// 頂点のデータ書き込み
	/// </summary>
	void VertexMap();

	/// <summary>
	/// インデックスのデータ書き込み
	/// </summary>
	void IndexMap();



public:
	/*=================================================================
	
								アクセッサ

	=================================================================*/

	/// 
	/// MeshData
	/// 
	const MeshData& GetMeshData() const { return meshData_; }
	MeshData& GetMeshData() { return meshData_; }
	/// 
	/// MeshResource
	/// 
	const MeshResource& GetMeshResource() const { return meshResources_; }

	/// 
	/// VertexData
	/// 
	VertexData* GetVertexData() const { return vertexData_; }

	/// 
	/// IndexData
	/// 
	uint32_t* GetIndexData() const { return indexData_; }

	/// 
	/// Material
	/// 
	uint32_t GetMaterialIndex() const { return meshData_.materialIndex; }
	void SetUseMaterialIndex(uint32_t index) { meshData_.materialIndex = index; }

	/// 
	/// DirectXCommon
	/// 
	DirectXCommon* GetDirectXCommon() const { return dxCommon_; }
	void SetDirectXCommon(DirectXCommon* dxCommon) { dxCommon_ = dxCommon; }
	

private:
	/*=================================================================

								ポインタ

	=================================================================*/
	DirectXCommon* dxCommon_ = nullptr;


	/*=================================================================

							メッシュリソース関連

	=================================================================*/
	MeshData meshData_;
	MeshResource meshResources_;
	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;


};

