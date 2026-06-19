#include "MaterialUV.h"
#include "DirectXCommon.h"

void MaterialUV::Initialize()
{
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(MaterialUVData));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&materialUV_));

	materialUV_->uvTransform = MakeIdentity4x4();
	materialUV_->stochasticStrength = 0.0f;
	materialUV_->_pad[0] = materialUV_->_pad[1] = materialUV_->_pad[2] = 0.0f;
}

void MaterialUV::RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV)
{
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndexCBV, resource_->GetGPUVirtualAddress());
}
