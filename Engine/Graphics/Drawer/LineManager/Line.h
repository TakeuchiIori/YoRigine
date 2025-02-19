#pragma once

// C++
#include <d3d12.h>
#include <wrl.h>
#include <array>

// Math
#include "Vector3.h"
#include "Matrix4x4.h"
#include "MathFunc.h"


class Camera;
class LineManager;
class DirectXCommon;
class Line
{

public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 線を描画
	/// </summary>
	void DrawLine();

	void ClearVertices();

	/// <summary>
	/// 頂点を作成
	/// </summary>
	void UpdateVertices(const Vector3& start, const Vector3& end);

private:



	/// <summary>
	/// 頂点リソース
	/// </summary>
	void CrateVetexResource();

	/// <summary>
	/// 頂点リソース
	/// </summary>
	void CrateMaterialResource();

	/// <summary>
	/// 座標変換リソース
	/// </summary>
	void CreateTransformResource();

public:

	void SetCamera(Camera* camera) { this->camera_ = camera; }

private:
	struct VertexData {
		Vector4 position;
	};

	struct MaterialData {
		Vector4 color;
		float padiing[3];
	};

	struct TransformationMatrix {
		Matrix4x4 WVP;
	};
	
	// 外部からのポインタ
	LineManager* lineManager_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// 頂点関連
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ ;
	D3D12_VERTEX_BUFFER_VIEW  vertexBufferView_ = {};
	VertexData* vertexData_ = nullptr;

	// マテリアル関連
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	MaterialData* materialData_ = nullptr;

	// 座標関連
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	TransformationMatrix* transformationMatrix_ = nullptr;

	const uint32_t kMaxNum = 4096u * 4u;
	uint32_t index = 0u;
	VertexData vertices_[2];

};

