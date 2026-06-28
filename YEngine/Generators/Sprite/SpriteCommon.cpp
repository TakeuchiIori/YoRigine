#include "SpriteCommon.h"
#include "PipelineManager/YPipelineManager.h"

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

/// <summary>
/// スプライト共通処理の初期化（ルートシグネチャ・PSO を設定）
/// </summary>
/// <param name="dxCommon">DirectX 共通管理クラス</param>
void SpriteCommon::Initialize(YoRigine::DirectXCommon* dxCommon)
{
	// DirectX共通クラスを記録
	dxCommon_ = dxCommon;

	// スプライト用のルートシグネチャを取得
	rootSignature_ = YPipelineManager::GetInstance()->GetRootSignature("Sprite");

	// スプライト描画用パイプラインステートを取得
	graphicsPipelineState_ = YPipelineManager::GetInstance()->GetPipeLineStateObject("Sprite");
}

/// <summary>
/// スプライト描画前の設定（PSO / ルートシグネチャ / トポロジ）
/// </summary>
void SpriteCommon::DrawPreference()
{
	// パイプラインステート設定
	SetGraphicsCommand();

	// ルートシグネチャ設定
	SetRootSignature();

	// プリミティブトポロジ設定（三角形）
	SetPrimitiveTopology();
}

/// <summary>
/// ルートシグネチャをコマンドリストへ設定
/// </summary>
void SpriteCommon::SetRootSignature()
{
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
}

/// <summary>
/// パイプラインステートオブジェクト（PSO）をコマンドリストへ設定
/// </summary>
void SpriteCommon::SetGraphicsCommand()
{
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());
}

/// <summary>
/// プリミティブトポロジ（三角形リスト）を設定
/// </summary>
void SpriteCommon::SetPrimitiveTopology()
{
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
