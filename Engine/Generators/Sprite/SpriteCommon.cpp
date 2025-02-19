#include "SpriteCommon.h"
#include "PipelineManager/PipelineManager.h"

// シングルトンインスタンスの初期化
std::unique_ptr<SpriteCommon> SpriteCommon::instance = nullptr;
std::once_flag SpriteCommon::initInstanceFlag;

/// <summary>
/// シングルトンのインスタンスを取得
/// </summary>
SpriteCommon* SpriteCommon::GetInstance()
{
	std::call_once(initInstanceFlag, []() {
		instance.reset(new SpriteCommon());
		});
	return instance.get();
}




void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
	// 引数で受け取ってメンバ変数に記録
	dxCommon_ = dxCommon;
	

	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("Sprite");
	graphicsPipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("Sprite");
}

void SpriteCommon::DrawPreference()
{
	SetGraphicsCommand();

	SetRootSignature();
	//PipelineManager::GetInstance()->SetPipeline("Sprite");
	SetPrimitiveTopology();

}

void SpriteCommon::SetRootSignature()
{

	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

}

void SpriteCommon::SetGraphicsCommand()
{
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());
}

void SpriteCommon::SetPrimitiveTopology()
{
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
