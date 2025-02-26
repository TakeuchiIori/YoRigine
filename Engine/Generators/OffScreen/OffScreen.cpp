#include "OffScreen.h"
#include "../Core/DX/DirectXCommon.h"
#include "PipelineManager/PipelineManager.h"
void OffScreen::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	/*rootSignature_  = PipelineManager::GetInstance()->GetRootSignature("OffScreen_GaussSmoothing");
	pipelineState_  = PipelineManager::GetInstance()->GetPipeLineStateObject("OffScreen_GaussSmoothing");*/
	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("OffScreen_OutLine");
	pipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("OffScreen_OutLine");

	//CreateBoxFilterResource();
	//CreateGaussFilterResource();
}


void OffScreen::Draw()
{
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, dxCommon_->GetOffScreenGPUHandle());

	//dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, gaussResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

void OffScreen::CreateBoxFilterResource()
{
	boxResource_ = dxCommon_->CreateBufferResource(sizeof(KernelForGPU));
	boxResource_->Map(0, nullptr, reinterpret_cast<void**>(&boxData_));
	boxResource_->Unmap(0, nullptr);
	boxData_->kernelSize = 5;
}

void OffScreen::CreateGaussFilterResource()
{
	gaussResource_ = dxCommon_->CreateBufferResource(sizeof(GaussKernelForGPU));
	gaussResource_->Map(0, nullptr, reinterpret_cast<void**>(&gaussData_));
	gaussResource_->Unmap(0, nullptr);
	gaussData_->kernelSize = 3;
	gaussData_->sigma = 2.0f;
}

