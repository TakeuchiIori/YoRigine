#include "MaterialDissolve.h"
#include "DirectXCommon.h"

void MaterialDissolve::Initialize()
{
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(DissolveData));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&data_));
	*data_ = DissolveData{};
}

void MaterialDissolve::RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT index)
{
	commandList->SetGraphicsRootConstantBufferView(index, resource_->GetGPUVirtualAddress());
}
