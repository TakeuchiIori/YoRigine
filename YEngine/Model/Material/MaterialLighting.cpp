#include "MaterialLighting.h"
#include "DirectXCommon.h"

void MaterialLighting::Initialize()
{
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(MaterialLight));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&materialLight_));
	materialLight_->enableLighting = true;
	materialLight_->shininess = 70.0f;
	materialLight_->enableSpecular = false;
	materialLight_->isHalfVector = false;
	materialLight_->enableEnvironment = false;
	materialLight_->environmentCoefficient = 1.0f;
}

void MaterialLighting::RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV)
{
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndexCBV, resource_->GetGPUVirtualAddress());
}
