#include "OffScreen.h"
#include "../Core/DX/DirectXCommon.h"
#include "PipelineManager/PipelineManager.h"
void OffScreen::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	rootSignature_  = PipelineManager::GetInstance()->GetRootSignature("OffScreen_Vignette");
	pipelineState_  = PipelineManager::GetInstance()->GetPipeLineStateObject("OffScreen_Vignette");
}


void OffScreen::Draw()
{
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, dxCommon_->GetOffScreenGPUHandle());
	dxCommon_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

