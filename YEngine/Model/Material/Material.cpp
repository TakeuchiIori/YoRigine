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

void Material::RecordDrawCommands(ID3D12GraphicsCommandList* command, UINT rootParameterIndexCBV, UINT rootParameterIndexSRV)
{
	command->SetGraphicsRootConstantBufferView(rootParameterIndexCBV, materialConstantResource_->GetGPUVirtualAddress());
	command->SetGraphicsRootDescriptorTable(rootParameterIndexSRV, TextureManager::GetInstance()->GetsrvHandleGPU(mtlData_.textureFilePath));
}


void Material::LoadTexture()
{
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
}
