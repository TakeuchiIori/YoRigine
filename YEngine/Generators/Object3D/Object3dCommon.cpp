#include "Object3dCommon.h"
#include "PipelineManager/YPipelineManager.h"
#include <LightManager/LightManager.h>

// シングルトンインスタンスの初期化
std::unique_ptr<Object3dCommon> Object3dCommon::instance = nullptr;
std::once_flag Object3dCommon::initInstanceFlag;


/// <summary>
/// シングルトンインスタンス取得
/// </summary>
Object3dCommon* Object3dCommon::GetInstance()
{
	std::call_once(initInstanceFlag, []() {
		instance.reset(new Object3dCommon());
		});
	return instance.get();
}


/// <summary>
/// 共通処理の初期化（ルートシグネチャ・PSO の取得）
/// </summary>
void Object3dCommon::Initialize(YoRigine::DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

	// Object3d 用パイプラインセット取得
	rootSignature_ = YPipelineManager::GetInstance()->GetRootSignature("Object");
	graphicsPipelineState_ = YPipelineManager::GetInstance()->GetPipeLineStateObject("Object");
}


/// <summary>
/// 描画前の共通設定（ルートシグネチャ・PSO・トポロジ）
/// </summary>
void Object3dCommon::DrawPreference()
{
	SetRootSignature();
	SetGraphicsCommand();
	SetPrimitiveTopology();
	YoRigine::LightManager::GetInstance()->SetCommandList();
}


/// <summary>
/// ルートシグネチャ設定
/// </summary>
void Object3dCommon::SetRootSignature()
{
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
}


/// <summary>
/// パイプラインステート設定
/// </summary>
void Object3dCommon::SetGraphicsCommand()
{
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());
}


/// <summary>
/// トポロジ設定（基本：三角形）
/// </summary>
void Object3dCommon::SetPrimitiveTopology()
{
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
