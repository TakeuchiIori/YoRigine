#include "OffScreen.h"
#include "../Core/DX/DirectXCommon.h"
#include "PipelineManager/PipelineManager.h"
void OffScreen::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	rootSignature_  = PipelineManager::GetInstance()->GetRootSignature("OffScreen_Smoothing");
	pipelineState_  = PipelineManager::GetInstance()->GetPipeLineStateObject("OffScreen_Smoothing");


	CreateSmoothResource();
}


void OffScreen::Draw()
{
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, dxCommon_->GetOffScreenGPUHandle());

	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, smoothResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

void OffScreen::CreateSmoothResource()
{
	smoothResource_ = dxCommon_->CreateBufferResource(sizeof(KernelForGPU));
	smoothResource_->Map(0, nullptr, reinterpret_cast<void**>(&smoothData_));
	smoothResource_->Unmap(0, nullptr);
	smoothData_->kernelSize = 5;
}

