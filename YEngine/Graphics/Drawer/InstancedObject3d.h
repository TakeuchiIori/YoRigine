#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Vector4.h"
#include "Matrix4x4.h"

class Camera;
class Model;
class SrvManager;

namespace YoRigine {
	class DirectXCommon;
}

/// <summary>
/// 非アニメ Object3d のインスタンシング描画クラス
///
/// 使い方:
///   InstancedObject3d::GetInstance()->Initialize();
///   ...
///   // 毎フレーム
///   inst->Begin();
///   for (auto* obj : visibleStaticObjects) {
///     InstancedObject3d::InstanceData d{};
///     d.world = obj->matWorld;
///     d.WIT = Transpose(Inverse(d.world));
///     d.WVP = d.world * camera->vp;
///     d.color = obj->color;
///     d.uvTransform = obj->uvMatrix;
///     d.stochasticStrength = obj->uvStochastic;
///     inst->AddInstance(obj->model, d);
///   }
///   inst->Flush(camera);
/// </summary>
class InstancedObject3d
{
public:
	// HLSL 側 (Object3dInstanced.VS/PS の InstanceData) と完全一致させる
	struct InstanceData {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
		Vector4   color = { 1.0f, 1.0f, 1.0f, 1.0f };
		Matrix4x4 uvTransform;
		float     stochasticStrength = 0.0f;
		float     _pad0 = 0.0f;
		float     _pad1 = 0.0f;
		float     _pad2 = 0.0f;
	};

	static InstancedObject3d* GetInstance();

	void Initialize();
	void Finalize();

	// フレーム開始: 全バッチをリセット
	void Begin();

	// インスタンスをモデル単位でバッファに積む
	void AddInstance(Model* model, const InstanceData& data);

	// カラーパス: ObjectInstanced PSO で全バッチを描画
	void Flush(Camera* camera);

	// 影パス: ShadowMapInstanced PSO で全バッチを描画 (gLight は外側で設定済み前提)
	void FlushShadow();

	// 直前フレームの統計
	uint32_t GetTotalInstances() const { return totalInstances_; }
	uint32_t GetBatchCount() const { return static_cast<uint32_t>(batches_.size()); }

private:
	InstancedObject3d() = default;
	~InstancedObject3d() = default;
	InstancedObject3d(const InstancedObject3d&) = delete;
	InstancedObject3d& operator=(const InstancedObject3d&) = delete;

	struct Batch {
		std::vector<InstanceData> cpuData;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpuBuffer;
		InstanceData* mapped = nullptr;
		uint32_t capacity = 0;
		uint32_t srvIndex = UINT32_MAX;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
	};

	// 指定batchが指定要素数を入れられるように容量確保 (足りなければ再作成)
	void EnsureCapacity(Batch& batch, uint32_t needed);

	// 共有 MaterialLight CB をデフォルト値で初期化
	void CreateMaterialLightCB();

private:
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	std::unordered_map<Model*, Batch> batches_;

	// 全インスタンス共有の MaterialLight CB (lighting有効, specular無効, env無効)
	Microsoft::WRL::ComPtr<ID3D12Resource> materialLightCB_;

	uint32_t totalInstances_ = 0;
};
