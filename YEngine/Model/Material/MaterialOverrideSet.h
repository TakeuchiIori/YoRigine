#pragma once

// C++
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

// Engine
#include "Material/Material.h"

// Math
#include "Vector3.h"

namespace YoRigine {
class Model;
}

///=============================================================================
/// メッシュ (マテリアルスロット) 1 枚ぶんのマテリアル上書き設定。
///
/// モデルは複数オブジェクトで共有されるため、Material
/// を直接書き換えると同じ
/// モデルを使う全オブジェクトに波及してしまう。この構造体は PlacedObject /
/// Object3d 側が持ち、描画時にだけモデル本来の値へ上書きを重ねる。
///
/// 各項目は「上書きするか」のフラグとセット。フラグが false
/// のものはモデル本来の値 (＝ Blender で設定した粗さ・メタリック)
/// がそのまま使われる。
///=============================================================================
struct MeshMaterialOverride {
  // ベースカラー (拡散反射色 Kd) の上書き。
  bool overrideBaseColor = false;
  Vector3 baseColor = {1.0f, 1.0f, 1.0f};

  // 空ならモデル本来のテクスチャを使う。
  std::string texturePath;

  // 定数バッファに影響する上書きが 1 つでもあるか
  bool HasConstantOverride() const { return overrideBaseColor; }

  // 何かしら上書きしているか (テクスチャ差し替えを含む)
  bool IsActive() const {
    return HasConstantOverride() || !texturePath.empty();
  }
};

///=============================================================================
/// オブジェクト 1 個ぶんの、全マテリアルスロットの上書きをまとめて持つクラス。
///
/// スロットごとに 256 バイト境界で並べた定数バッファを 1 本だけ確保し、
/// 描画直前の Apply() でモデル本来の値とマージして書き込む。
/// Material::MaterialConstant と同じレイアウトなので、描画側は
/// バインド先アドレスを差し替えるだけでよい。
///=============================================================================
class MaterialOverrideSet {
public:
  // CBV のアドレスは 256 バイト境界でなければならない
  static constexpr size_t kSlotStride = 256;

  MaterialOverrideSet() = default;
  ~MaterialOverrideSet() = default;
  MaterialOverrideSet(const MaterialOverrideSet &) = delete;
  MaterialOverrideSet &operator=(const MaterialOverrideSet &) = delete;

  // スロット数を確保する (モデルのマテリアル数で呼ぶ)。既存の値は保持される。
  void EnsureSlotCount(size_t count);

  size_t GetSlotCount() const { return slots_.size(); }
  std::vector<MeshMaterialOverride> &GetSlots() { return slots_; }
  const std::vector<MeshMaterialOverride> &GetSlots() const { return slots_; }

  MeshMaterialOverride *GetSlot(size_t index);
  const MeshMaterialOverride *GetSlot(size_t index) const;

  // 値を書き換えたら呼ぶ。次の Apply() で GPU へ反映される。
  void MarkDirty() { dirty_ = true; }

  // テクスチャ差し替え。TextureManager
  // へのロードまで面倒を見るのでこちらを使う。
  // 空文字でモデル本来のテクスチャに戻る。
  void SetSlotTexture(size_t index, const std::string &path);

  // 1 つでも上書きがあるか。false ならこのセットを渡す必要はない。
  bool HasAnyOverride() const;

  // 全スロットの上書きを解除する
  void ClearAll();

  // モデル本来の値とマージして GPU へ書き込む。描画直前に呼ぶ (dirty
  // 時のみ実働)。
  void Apply(YoRigine::Model &model);

  // slot の定数バッファアドレス。上書きが無ければ 0 (＝モデル本来の CB
  // を使う)。
  D3D12_GPU_VIRTUAL_ADDRESS GetSlotConstantAddress(size_t index) const;

  // slot のテクスチャ上書きパス。空ならモデル本来のテクスチャ。
  const std::string &GetSlotTexturePath(size_t index) const;

  // 内容が変わるたびに増える番号。インスタンシングのバッチ分割キーに使う。
  uint64_t GetRevision() const { return revision_; }

private:
  void EnsureBuffer(size_t slotCount);

  std::vector<MeshMaterialOverride> slots_;

  Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
  uint8_t *mapped_ = nullptr;
  size_t bufferSlotCapacity_ = 0;

  bool dirty_ = true;
  uint64_t revision_ = 0;
};
