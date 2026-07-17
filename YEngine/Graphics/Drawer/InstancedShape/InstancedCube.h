#pragma once

// C++
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

// Math
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"

class Camera;

namespace YoRigine {
	class DirectXCommon;
	class SrvManager;
}

// ============================================================
// InstancedCube
//   - 立方体ワイヤーフレーム (12 エッジ = 24 頂点) を1つだけVBに持つ
//   - 各インスタンスは (worldMat, color) を StructuredBuffer に積む
//   - 1 DrawInstanced で全インスタンスを描画する
//
// 使い方:
//   inst.Initialize();
//   inst.SetCamera(camera);
//   inst.Begin();
//   inst.AddAABB(min, max, color);     // 軸平行ボックス
//   inst.AddOBB(center, euler, size, color); // 回転付きボックス
//   inst.AddCube(worldMat, color);     // 任意行列
//   inst.Flush();   // 1 DrawInstanced で全部描画
//
// Note:
//   - 単位立方体は ±1 の頂点 (full size 2)。スケール = halfSize でフィット。
//   - OBB.size, AABB の min/max もこの「±1 = halfSize」に合わせて変換する。
// ============================================================
class InstancedCube
{
public:
	void Initialize();

	void SetCamera(Camera* camera) { camera_ = camera; }

	// フレーム開始時にカウンタリセット
	void Begin();

	// 軸平行ボックス追加
	void AddAABB(const Vector3& mn, const Vector3& mx, const Vector4& color);
	// 回転付きボックス追加 (size は半サイズ)
	void AddOBB(const Vector3& center, const Vector3& eulerRot, const Vector3& size, const Vector4& color);
	// 任意 worldMat の立方体追加
	void AddCube(const Matrix4x4& worldMat, const Vector4& color);

	// 蓄積分を 1 DrawInstanced で描画
	void Flush();

	uint32_t GetInstanceCount() const { return instanceCount_; }

private:
	void CreateVertexBuffer();
	void CreateInstanceBuffer();
	void CreateVPBuffer();

	struct InstanceData {
		Matrix4x4 worldMat;
		Vector4   color;
	};

	struct VertexData {
		Vector4 position;
	};

	static constexpr uint32_t kCubeVertices = 24; // 12 edges * 2 verts
	static constexpr uint32_t kMaxInstances = 4096;

	YoRigine::DirectXCommon* dxCommon_   = nullptr;
	YoRigine::SrvManager*    srvManager_ = nullptr;
	Camera*                  camera_     = nullptr;

	// 単位立方体 VB
	Microsoft::WRL::ComPtr<ID3D12Resource> vbResource_;
	D3D12_VERTEX_BUFFER_VIEW               vbView_{};

	// インスタンス SB
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_;
	InstanceData*                          instanceData_ = nullptr;
	uint32_t                               srvIndex_     = 0;
	uint32_t                               instanceCount_ = 0;

	// VP 行列 CBV
	Microsoft::WRL::ComPtr<ID3D12Resource> vpResource_;
	Matrix4x4*                             vpData_ = nullptr;
};
