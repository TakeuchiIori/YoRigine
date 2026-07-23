#include "Material.h"
#include "DirectXCommon.h"
#include <Loaders/Texture/TextureManager.h>
void Material::Initialize(std::string& textureFilePath)
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	textureFilePath_ = textureFilePath;

	SetTextureFilePath(textureFilePath_);

	LoadTexture();

	materialConstantResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialConstant));
	materialConstantResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialConstant_));
	materialConstant_->Kd = mtlData_.Kd;
}

void Material::RecordDrawCommands(ID3D12GraphicsCommandList* command, UINT rootParameterIndexCBV, UINT rootParameterIndexSRV, const std::string& overrideTexturePath)
{
	command->SetGraphicsRootConstantBufferView(rootParameterIndexCBV, materialConstantResource_->GetGPUVirtualAddress());
	// 上書き指定があればそのテクスチャを、無ければ本来のテクスチャをバインドする
	const std::string& texturePath = overrideTexturePath.empty() ? mtlData_.textureFilePath : overrideTexturePath;
	command->SetGraphicsRootDescriptorTable(rootParameterIndexSRV, TextureManager::GetInstance()->GetsrvHandleGPU(texturePath));
}


void Material::LoadTexture()
{
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
}
