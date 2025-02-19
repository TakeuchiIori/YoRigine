#include "LineManager.h"
#include "DX./DirectXCommon.h"
#include "PipelineManager/PipelineManager.h"


LineManager* LineManager::GetInstance()
{
	static LineManager instance;
	return &instance;
}

void LineManager::Initialize()
{
	// ポインタを渡す
	dxCommon_ = DirectXCommon::GetInstance();
	
	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("Line");
	graphicsPipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("Line");

}