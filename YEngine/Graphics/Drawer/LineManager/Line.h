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

namespace YoRigine {
	class DirectXCommon;
}

/// <summary>
/// ライン描画クラス
/// </summary>
class Line
{

public:
	///************************* 基本関数 *************************///

	void Initialize();
	void DrawLine();

	// 頂点の登録
	void RegisterLine(const Vector3& start, const Vector3& end);

	// 各形状の描画
	void DrawSphere(const Vector3& center, float radius, int resolution);
	void DrawCircleXZ(const Vector3& center, float radius, int resolution);
	void DrawAABB(const Vector3& min, const Vector3& max);
	void DrawOBB(const Vector3& center, const Vector3& rotationEuler, const Vector3& size);
	void DrawCone(const Vector3& position, float rotationY,float viewDistance, float viewAngleDeg, int resolution = 16);
private:
	///************************* 内部処理 *************************///

	// 頂点リソース
	void CrateVetexResource();
	// マテリアルリソース
	void CrateMaterialResource();
	// 座標変換リソース
	void CreateTransformResource();
public:
	///************************* アクセッサ *************************///
	void SetCamera(Camera* camera) { this->camera_ = camera; }

private:
	///************************* GPU用の構造体 *************************///
	// 頂点データ構造体
	struct VertexData {
		Vector4 position;
	};
	// マテリアル構造体
	struct MaterialData {
		Vector4 color;
		float padiing[3];
	};
	// 座標変換構造体
	struct TransformationMatrix {
		Matrix4x4 WVP;
	};
private:
	///************************* メンバ変数 *************************///
	LineManager* lineManager_ = nullptr;
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// 頂点関連
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW  vertexBufferView_ = {};
	VertexData* vertexData_ = nullptr;

	// マテリアル関連
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	MaterialData* materialData_ = nullptr;

	// 座標関連
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	TransformationMatrix* transformationMatrix_ = nullptr;

	// 定数
	const uint32_t kMaxNum = 4096u * 4u;
	uint32_t index = 0u;
	VertexData vertices_[2];

};

