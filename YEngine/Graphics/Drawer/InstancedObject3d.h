#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>

#include "Vector4.h"
#include "Matrix4x4.h"
#include "Material/MaterialLighting.h"
#include "Object3D/ObjectManager.h"   // PlacedObject (ネスト型なので前方宣言不可)

class Camera;
class Model;
class SrvManager;
class WorldTransform;
class Object3d;

namespace YoRigine {
	class DirectXCommon;
}

/// <summary>
/// 非アニメ Object3d のインスタンシング描画クラス
///
/// 推奨の使い方 (WVP / WIT / 材質は Submit が内部で処理する):
///   InstancedObject3d::GetInstance()->Initialize();
///   ...
///   // 毎フレーム
///   inst->Begin(camera);                     // このフレームのカメラを渡す
///   // PlacedObject を積む場合 (ModelManipulator 等)
///   for (auto* obj : placedObjects)  inst->Submit(*obj);
///   // 素の Object3d を積む場合 (WorldTransform は別途渡す)
///   for (auto& e : enemies)          inst->Submit(e.object, e.worldTransform);
///   inst->DrawAll(camera);
///
/// 低レベル版 (WVP / WIT を自前で埋めたい場合):
///   inst->Begin();
///   InstancedObject3d::InstanceData d{}; ... inst->AddInstance(model, d);
///   inst->DrawAll(camera);
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
		float     outlineMask = 1.0f;   // インバートハル輪郭線の掛け率(1=線あり, 0=線なし)
		float     _pad1 = 0.0f;
		float     _pad2 = 0.0f;
	};

	// Submit 用の入力。派生行列 (WVP / WIT) は内部で計算するので渡さない。
	// 必要な値だけ指定すればよいよう既定値付き。
	struct Instance {
		Matrix4x4 world;
		Vector4   color = { 1.0f, 1.0f, 1.0f, 1.0f };
		Matrix4x4 uvTransform = MakeIdentity4x4();
		float     stochasticStrength = 0.0f;
		float     outlineMask = 1.0f;   // 1=輪郭線あり, 0=なし
	};

	static InstancedObject3d* GetInstance();

	void Initialize();
	void Finalize();

	// フレーム開始: 全バッチをリセット
	void Begin();

	// フレーム開始 + このフレームのカメラを保持 (Submit の WVP 計算に使う)
	void Begin(Camera* camera);

	// 低レベル: 生データを渡すだけ。WIT = Transpose(Inverse(world)) は内部計算。
	// WVP は直前の Begin(camera) のカメラで計算 (未指定なら World をそのまま入れる=影パス用)。
	void Submit(Model* model, const Instance& src);

	// PlacedObject 用: model / world / 色 / UV / 輪郭線をすべて PlacedObject から取り出して積む。
	// 影パスでも同じ呼び出しでよい (Begin() でカメラ未指定なら WVP=World、材質は影で無視される)。
	void Submit(const ObjectManager::PlacedObject& placed);

	// Object3d 用: Object3d は自前の WorldTransform を持たないので transform を別途渡す。
	// 材質 (色 / UV / stochastic / 輪郭線) は Object3d から取り出す。
	void Submit(Object3d& object, const WorldTransform& transform);

	// 低レベル: WVP / WIT を含む完成済みデータを積む
	void AddInstance(Model* model, const InstanceData& data);

	// カラーパス: ObjectInstanced PSO で全バッチを描画
	void DrawAll(Camera* camera);

	// 影パス: ShadowMapInstanced PSO で全バッチを描画 (gLight は外側で設定済み前提)
	void DrawShadow();

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

private:
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// Begin(camera) で保持する、このフレームの Submit 用カメラ
	Camera* frameCamera_ = nullptr;

	std::unordered_map<Model*, Batch> batches_;

	// 全インスタンス共有の MaterialLight (lighting有効, specular無効, env無効)。
	// MaterialLighting 経由にすることで、トゥーン等のグローバル設定が自動反映される。
	std::unique_ptr<MaterialLighting> materialLighting_;

	uint32_t totalInstances_ = 0;
};
