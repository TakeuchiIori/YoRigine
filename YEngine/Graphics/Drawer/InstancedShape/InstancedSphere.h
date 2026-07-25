#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"

namespace YoRigine { class Camera; }

namespace YoRigine {
	class DirectXCommon;
	class SrvManager;
}

// ============================================================
// InstancedSphere
//   - 単位球ワイヤーフレーム (3 great circles) を 1 つだけ VB に持つ
//   - 各インスタンスは (worldMat, color) を StructuredBuffer に積む
//   - 1 DrawInstanced で全インスタンスを描画
//   - InstancedCube パイプライン ("InstancedCube") をシェーダーが同型のため再利用
// ============================================================
class InstancedSphere
{
public:
	void Initialize();
	void SetCamera(YoRigine::Camera* camera) { camera_ = camera; }

	void Begin();

	// 中心 + 半径 + 色 (内部で worldMat に変換)
	void AddSphere(const Vector3& center, float radius, const Vector4& color);

	// 任意 worldMat
	void AddSphereMat(const Matrix4x4& worldMat, const Vector4& color);

	void Flush();

	uint32_t GetInstanceCount() const { return instanceCount_; }

private:
	void BuildUnitSphereVertices();
	void CreateInstanceBuffer();
	void CreateVPBuffer();

	struct InstanceData {
		Matrix4x4 worldMat;
		Vector4   color;
	};

	struct VertexData {
		Vector4 position;
	};

	// 3 平面 × resolution セグメント × 2 頂点 / セグメント
	static constexpr uint32_t kResolution    = 16;
	static constexpr uint32_t kVerticesCount = 3 * kResolution * 2;
	static constexpr uint32_t kMaxInstances  = 4096;

	YoRigine::DirectXCommon* dxCommon_   = nullptr;
	YoRigine::SrvManager*    srvManager_ = nullptr;
	YoRigine::Camera*                  camera_     = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vbResource_;
	D3D12_VERTEX_BUFFER_VIEW               vbView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_;
	InstanceData*                          instanceData_ = nullptr;
	uint32_t                               srvIndex_     = 0;
	uint32_t                               instanceCount_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> vpResource_;
	Matrix4x4*                             vpData_ = nullptr;
};
