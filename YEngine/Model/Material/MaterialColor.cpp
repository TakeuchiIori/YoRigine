#include "MaterialColor.h"
#include "DirectXCommon.h"

void MaterialColor::Initialize()
{
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(ColorData));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&colorData_));
	colorData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

void MaterialColor::RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT index)
{
	commandList->SetGraphicsRootConstantBufferView(index, resource_->GetGPUVirtualAddress());
}
