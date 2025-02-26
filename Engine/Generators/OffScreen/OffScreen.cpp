#include "OffScreen.h"
#include "../Core/DX/DirectXCommon.h"
#include "PipelineManager/PipelineManager.h"
void OffScreen::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	/*rootSignature_  = PipelineManager::GetInstance()->GetRootSignature("OffScreen_GaussSmoothing");
	pipelineState_  = PipelineManager::GetInstance()->GetPipeLineStateObject("OffScreen_GaussSmoothing");*/
	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("OffScreen_DepthOutLine");
	pipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("OffScreen_DepthOutLine");

	//CreateBoxFilterResource();
	//CreateGaussFilterResource();
	CreateMaterialResource();
}


void OffScreen::Draw()
{
	materialData_->Inverse = Inverse(projectionInverse_);
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, dxCommon_->GetOffScreenGPUHandle());

	//dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, gaussResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(1, dxCommon_->GetDepthGPUHandle());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(2, materialResource_->GetGPUVirtualAddress());
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

void OffScreen::CreateMaterialResource()
{
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialResource_->Unmap(0, nullptr);
	materialData_->Inverse = MakeIdentity4x4();
	materialData_->kernelSize = 3;
	materialData_->outlineColor = { 1.0f, 0.0f, 0.0f, 1.0f };
}

