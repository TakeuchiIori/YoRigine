#include "Object3dCommon.h"
#include "PipelineManager/PipelineManager.h"
// シングルトンインスタンスの初期化
std::unique_ptr<Object3dCommon> Object3dCommon::instance = nullptr;
std::once_flag Object3dCommon::initInstanceFlag;

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
Object3dCommon* Object3dCommon::GetInstance()
{
	std::call_once(initInstanceFlag, []() {
		instance.reset(new Object3dCommon());
		});
	return instance.get();
}

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	rootSignature_ = PipelineManager::GetInstance()->GetRootSignature("Object");
	graphicsPipelineState_ = PipelineManager::GetInstance()->GetPipeLineStateObject("Object");
}

void Object3dCommon::DrawPreference()
{
	//// ルートシグネチャをセット
	SetRootSignature();

	//// パイプラインをセット
	SetGraphicsCommand();

	// プリミティブトポロジーをセット
	SetPrimitiveTopology();
}

void Object3dCommon::SetRootSignature()
{
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
}

void Object3dCommon::SetGraphicsCommand()
{
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());
}

void Object3dCommon::SetPrimitiveTopology()
{
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
