#pragma once

#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

#include "Material/MaterialLighting.h"
#include "Matrix4x4.h"
#include "Object3D/ObjectManager.h" // PlacedObject (ネスト型なので前方宣言不可)
#include "Vector4.h"

namespace YoRigine {
class Camera;
}
namespace YoRigine {
class Model;
}
namespace YoRigine {
class WorldTransform;
}
namespace YoRigine {
class Object3d;
}

namespace YoRigine {
class DirectXCommon;
class SrvManager;
} // namespace YoRigine

/// <summary>
/// 非アニメ Object3d のインスタンシング描画クラス
///
/// 推奨の使い方 (WVP / WIT / 材質は Submit が内部で処理する):
///   InstancedObject3d::GetInstance()->Initialize();
///   ...
///   // 毎フレーム
///   inst->Begin(camera);                     // このフレームのカメラを渡す
///   // PlacedObject を積む場合 (SceneEditor 等)
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
class InstancedObject3d {
public:
  // HLSL 側 (Object3dInstanced.VS/PS の InstanceData) と完全一致させる
  struct InstanceData {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Matrix4x4 uvTransform;
    float stochasticStrength = 0.0f;
    float outlineMask =
        1.0f;                // インバートハル輪郭線の掛け率(1=線あり, 0=線なし)
    float ditherFade = 0.0f; // 1=alpha をスクリーンドア透過率として使用
    float _pad2 = 0.0f;
  };

  // Submit 用の入力。派生行列 (WVP / WIT) は内部で計算するので渡さない。
  // 必要な値だけ指定すればよいよう既定値付き。
  struct Instance {
    Matrix4x4 world;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Matrix4x4 uvTransform = MakeIdentity4x4();
    float stochasticStrength = 0.0f;
    float outlineMask = 1.0f; // 1=輪郭線あり, 0=なし
    float ditherFade = 0.0f;  // 1=alpha をディザー透過率として使用
  };

  static InstancedObject3d *GetInstance();

  void Initialize();
  void Finalize();

  // フレーム開始: 全バッチをリセット
  void Begin();

  // フレーム開始 + このフレームのカメラを保持 (Submit の WVP 計算に使う)
  void Begin(YoRigine::Camera *camera);

  // 低レベル: 生データを渡すだけ。WIT = Transpose(Inverse(world)) は内部計算。
  // WVP は直前の Begin(camera) のカメラで計算 (未指定なら World
  // をそのまま入れる=影パス用)。
  // overrideTexturePath: 空でなければモデル既定の代わりにこのテクスチャを貼る。
  // 同一モデルでもテクスチャが異なると別バッチとして描画される。
  void Submit(YoRigine::Model *model, const Instance &src,
              const std::string &overrideTexturePath = "");

  // PlacedObject 用: model / world / 色 / UV / 輪郭線をすべて PlacedObject
  // から取り出して積む。 影パスでも同じ呼び出しでよい (Begin()
  // でカメラ未指定なら WVP=World、材質は影で無視される)。
  void Submit(const ObjectManager::PlacedObject &placed);

  // Object3d 用: Object3d は自前の WorldTransform を持たないので transform
  // を別途渡す。 材質 (色 / UV / stochastic / 輪郭線) は Object3d
  // から取り出す。
  void Submit(YoRigine::Object3d &object,
              const YoRigine::WorldTransform &transform);

  // 低レベル: WVP / WIT を含む完成済みデータを積む。
  // overrideTexturePath が空でなければテクスチャ別のバッチに積む。
  void AddInstance(YoRigine::Model *model, const InstanceData &data,
                   const std::string &overrideTexturePath = "");

  // カラーパス: ObjectInstanced PSO で全バッチを描画
  void DrawAll(YoRigine::Camera *camera);

  // カメラ遮蔽用: 深度書き込みを維持したままディザーで面を間引く。
  // 複雑なモデルの面同士が重なって崩れない。
  void SubmitDitherFade(const ObjectManager::PlacedObject &placed);

  // 影パス: ShadowMapInstanced PSO で全バッチを描画 (gLight
  // は外側で設定済み前提)
  void DrawShadow();

  // 直前フレームの統計
  uint32_t GetTotalInstances() const { return totalInstances_; }
  uint32_t GetBatchCount() const {
    return static_cast<uint32_t>(batches_.size());
  }

private:
  InstancedObject3d() = default;
  ~InstancedObject3d() = default;
  InstancedObject3d(const InstancedObject3d &) = delete;
  InstancedObject3d &operator=(const InstancedObject3d &) = delete;

  // バッチ識別子: 同じモデルでもテクスチャ上書きが違えば別バッチにする。
  // (インスタンシングは 1 ドロー = 1 テクスチャのため、テクスチャごとに分ける)
  struct BatchKey {
    YoRigine::Model *model = nullptr;
    std::string overrideTexturePath;
    bool operator==(const BatchKey &o) const {
      return model == o.model && overrideTexturePath == o.overrideTexturePath;
    }
  };
  struct BatchKeyHash {
    size_t operator()(const BatchKey &k) const {
      return std::hash<const void *>()(k.model) ^
             (std::hash<std::string>()(k.overrideTexturePath) << 1);
    }
  };

  struct Batch {
    std::vector<InstanceData> cpuData;

    struct BufferSlot {
      Microsoft::WRL::ComPtr<ID3D12Resource> gpuBuffer;
      InstanceData *mapped = nullptr;
      uint32_t capacity = 0;
      uint32_t srvIndex = UINT32_MAX;
      D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
    };

    std::vector<BufferSlot> colorSlots;
    uint32_t colorFrameIndex = UINT32_MAX;
    size_t nextColorSlot = 0;

    std::vector<BufferSlot> shadowSlots;
    uint32_t shadowFrameIndex = UINT32_MAX;
    size_t nextShadowSlot = 0;
  };

  // 指定batchが指定要素数を入れられるように容量確保 (足りなければ再作成)
  void EnsureCapacity(Batch::BufferSlot &slot, uint32_t needed);

  // 同一フレーム内の複数 DrawAll/DrawShadow が GPU 実行前に互いのデータを
  // 上書きしないよう、呼び出しごとに別スロットを使う。
  void EnsureShadowCapacity(Batch::BufferSlot &slot, uint32_t needed);
  Batch::BufferSlot &AcquireColorSlot(Batch &batch, uint32_t needed);
  Batch::BufferSlot &AcquireShadowSlot(Batch &batch, uint32_t needed);

private:
  YoRigine::DirectXCommon *dxCommon_ = nullptr;
  YoRigine::SrvManager *srvManager_ = nullptr;

  // Begin(camera) で保持する、このフレームの Submit 用カメラ
  YoRigine::Camera *frameCamera_ = nullptr;

  std::unordered_map<BatchKey, Batch, BatchKeyHash> batches_;

  // 全インスタンス共有の MaterialLight (lighting有効, specular無効, env無効)。
  // MaterialLighting
  // 経由にすることで、トゥーン等のグローバル設定が自動反映される。
  std::unique_ptr<MaterialLighting> materialLighting_;

  uint32_t totalInstances_ = 0;
};
