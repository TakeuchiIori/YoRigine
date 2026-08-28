#include "MaterialOverrideSet.h"

// Engine
#include "DirectXCommon.h"
#include "Model.h"
#include <Loaders/Texture/TextureManager.h>

namespace {
// GetSlotTexturePath が範囲外で返す空文字の実体
const std::string kEmptyPath;
} // namespace

void MaterialOverrideSet::EnsureSlotCount(size_t count) {
  if (slots_.size() >= count) {
    return;
  }
  slots_.resize(count);
  dirty_ = true;
}

MeshMaterialOverride *MaterialOverrideSet::GetSlot(size_t index) {
  return index < slots_.size() ? &slots_[index] : nullptr;
}

const MeshMaterialOverride *MaterialOverrideSet::GetSlot(size_t index) const {
  return index < slots_.size() ? &slots_[index] : nullptr;
}

void MaterialOverrideSet::SetSlotTexture(size_t index,
                                         const std::string &path) {
  if (index >= slots_.size()) {
    return;
  }
  slots_[index].texturePath = path;
  // 描画時に GetsrvHandleGPU で引けるよう、ここで読み込んでおく。
  if (!path.empty()) {
    TextureManager::GetInstance()->LoadTexture(path);
  }
  MarkDirty();
}

bool MaterialOverrideSet::HasAnyOverride() const {
  for (const auto &slot : slots_) {
    if (slot.IsActive()) {
      return true;
    }
  }
  return false;
}

void MaterialOverrideSet::ClearAll() {
  for (auto &slot : slots_) {
    slot = MeshMaterialOverride{};
  }
  MarkDirty();
}

void MaterialOverrideSet::EnsureBuffer(size_t slotCount) {
  if (mapped_ && bufferSlotCapacity_ >= slotCount) {
    return;
  }
  auto *dxCommon = YoRigine::DirectXCommon::GetInstance();
  if (!dxCommon) {
    return;
  }
  // 既存バッファは ComPtr の代入で解放される。GPU
  // 実行中の書き換えを避けるため
  // 拡張は「スロット数が増えたとき」だけで、通常は初回のみ走る。
  constantResource_ = dxCommon->CreateBufferResource(slotCount * kSlotStride);
  if (!constantResource_) {
    mapped_ = nullptr;
    bufferSlotCapacity_ = 0;
    return;
  }
  constantResource_->Map(0, nullptr, reinterpret_cast<void **>(&mapped_));
  bufferSlotCapacity_ = slotCount;
}

void MaterialOverrideSet::Apply(YoRigine::Model &model) {
  EnsureSlotCount(model.GetMaterialCount());
  if (slots_.empty() || !dirty_) {
    return;
  }

  EnsureBuffer(slots_.size());
  if (!mapped_) {
    return;
  }

  for (size_t i = 0; i < slots_.size(); ++i) {
    const MeshMaterialOverride &ov = slots_[i];
    if (!ov.HasConstantOverride()) {
      continue;
    }

    // モデル本来の値 (＝読み込んだ Kd) をベースに、
    // 上書きフラグが立っている項目だけ差し替える。
    Material *material = model.GetMaterial(i);
    Material::MaterialConstant c =
        material ? material->BuildConstant() : Material::MaterialConstant{};

    if (ov.overrideBaseColor) {
      c.Kd = ov.baseColor;
    }

    *reinterpret_cast<Material::MaterialConstant *>(mapped_ + i * kSlotStride) =
        c;
  }

  dirty_ = false;
  ++revision_;
}

D3D12_GPU_VIRTUAL_ADDRESS
MaterialOverrideSet::GetSlotConstantAddress(size_t index) const {
  if (index >= slots_.size() || !constantResource_ ||
      index >= bufferSlotCapacity_) {
    return 0;
  }
  if (!slots_[index].HasConstantOverride()) {
    return 0; // モデル本来の定数バッファを使う
  }
  return constantResource_->GetGPUVirtualAddress() + index * kSlotStride;
}

const std::string &MaterialOverrideSet::GetSlotTexturePath(size_t index) const {
  return index < slots_.size() ? slots_[index].texturePath : kEmptyPath;
}
