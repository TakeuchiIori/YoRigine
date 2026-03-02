#include "MaterialUV.h"
#include "DirectXCommon.h"

void MaterialUV::Initialize()
{
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(MaterialUVData));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&materialUV_));

	materialUV_->uvTransform = MakeIdentity4x4();

}

void MaterialUV::RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV)
{
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndexCBV, resource_->GetGPUVirtualAddress());
}
