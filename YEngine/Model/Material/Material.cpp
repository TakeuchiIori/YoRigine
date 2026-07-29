#include "Material.h"
#include "DirectXCommon.h"
#include <Loaders/Texture/TextureManager.h>

void Material::Initialize(std::string &textureFilePath) {
  dxCommon_ = YoRigine::DirectXCommon::GetInstance();
  textureFilePath_ = textureFilePath;

  SetTextureFilePath(textureFilePath_);

  LoadTexture();

  // ルート CBV は 256 バイト境界で読まれるため、構造体が 48 バイトでも
  // 確保は 256 バイトに切り上げる。境界をまたいだ読み出しで
  // D3D12 デバッグレイヤーが警告を出すのを防ぐ。
  constexpr size_t kConstantBufferAlignment = 256;
  constexpr size_t kConstantBufferSize =
      (sizeof(MaterialConstant) + kConstantBufferAlignment - 1) /
      kConstantBufferAlignment * kConstantBufferAlignment;
  materialConstantResource_ =
      dxCommon_->CreateBufferResource(kConstantBufferSize);
  materialConstantResource_->Map(0, nullptr,
                                 reinterpret_cast<void **>(&materialConstant_));
  UploadConstants();
}

Material::MaterialConstant Material::BuildConstant() const {
  MaterialConstant c{};
  c.Kd = mtlData_.Kd;
  return c;
}

void Material::UploadConstants() {
  if (!materialConstant_)
    return;
  *materialConstant_ = BuildConstant();
}

void Material::RecordDrawCommands(
    ID3D12GraphicsCommandList *command, UINT rootParameterIndexCBV,
    UINT rootParameterIndexSRV, const std::string &overrideTexturePath,
    D3D12_GPU_VIRTUAL_ADDRESS overrideConstantCB) {
  // 上書き CB があればそちらを、無ければ自分の定数バッファをバインドする
  command->SetGraphicsRootConstantBufferView(
      rootParameterIndexCBV,
      overrideConstantCB != 0
          ? overrideConstantCB
          : materialConstantResource_->GetGPUVirtualAddress());
  // 上書き指定があればそのテクスチャを、無ければ本来のテクスチャをバインドする
  const std::string &texturePath = overrideTexturePath.empty()
                                       ? mtlData_.textureFilePath
                                       : overrideTexturePath;
  command->SetGraphicsRootDescriptorTable(
      rootParameterIndexSRV,
      TextureManager::GetInstance()->GetsrvHandleGPU(texturePath));
}

void Material::LoadTexture() {
  TextureManager::GetInstance()->LoadTexture(textureFilePath_);
}
