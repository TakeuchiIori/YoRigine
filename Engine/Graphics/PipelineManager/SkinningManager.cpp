#include "SkinningManager.h"
#include "PipelineManager/PipelineManager.h"
// シングルトンインスタンスの初期化
std::unique_ptr<SkinningManager> SkinningManager::instance = nullptr;
std::once_flag SkinningManager::initInstanceFlag;

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
SkinningManager* SkinningManager::GetInstance()
{
	std::call_once(initInstanceFlag, []() {
		instance.reset(new SkinningManager());
		});
	return instance.get();
}

void SkinningManager::Initialize(DirectXCommon* dxCommon)
{
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("Animation");
	graphicsPipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("Animation");
}



void SkinningManager::DrawPreference()
{
	// ルートシグネチャをセット
	SetRootSignature();

	// パイプラインをセット
	SetGraphicsCommand();

	// プリミティブトポロジーをセット
	SetPrimitiveTopology();
}

void SkinningManager::SetRootSignature()
{
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
}

void SkinningManager::SetGraphicsCommand()
{
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());
}

void SkinningManager::SetPrimitiveTopology()
{
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
