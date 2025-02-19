#include "Material.h"
#include "../Core/DX/DirectXCommon.h"
#include <Loaders/Texture/TextureManager.h>
void Material::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();

	TransferData();

	LoadTexture();
}

void Material::TransferData()
{
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = true;
	materialData_->shininess = 30.0f;
	materialData_->uvTransform = MakeIdentity4x4();
}

void Material::LoadTexture()
{
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
}
