#include "CubeMap.h"
#include "Loaders/Texture/TextureManager.h"
#include "Loaders/Texture/EnvironmentMap.h"
#include "PipelineManager/YPipelineManager.h"

/// <summary>
/// キューブマップの初期化（メッシュ生成・テクスチャ読み込み・行列準備）
/// </summary>
namespace YoRigine {
	void CubeMap::Initialize(YoRigine::Camera* camera, std::string textureFilePath)
	{
		camera_ = camera;
		textureFilePath_ = textureFilePath;

		// 環境マップテクスチャ読み込み
		EnvironmentMap::GetInstance()->LoadEnvironmentTexture(textureFilePath_);

		// メッシュ生成
		CreateMesh();

		// 行列初期化
		wt_.Initialize();
		wt_.rotate_.x = 4.7f;   // 天球の向き調整

		// マテリアルカラー
		materialColor_ = std::make_unique<MaterialColor>();
		materialColor_->Initialize();
	}

	/// <summary>
	/// 行列更新
	/// </summary>
	void CubeMap::Update()
	{
		wt_.UpdateMatrix();
	}

	/// <summary>
	/// キューブマップ描画処理
	/// </summary>
	void CubeMap::Draw()
	{
		Matrix4x4 worldMatrix{};
		Matrix4x4 worldViewProjectionMatrix{};

		//-----------------------------------------
		// ワールド＆WVP 行列生成
		//-----------------------------------------
		if (camera_) {
			const Matrix4x4& viewProj = camera_->GetViewProjectionMatrix();
			worldMatrix = wt_.GetMatWorld();
			worldViewProjectionMatrix = worldMatrix * viewProj;
		}

		// GPU へ行列転送
		wt_.SetMapWVP(worldViewProjectionMatrix);
		wt_.SetMapWorld(worldMatrix);

		//-----------------------------------------
		// パイプライン設定
		//-----------------------------------------
		auto* dx = DirectXCommon::GetInstance();
		auto* cmd = dx->GetCommandList().Get();

		cmd->SetGraphicsRootSignature(YPipelineManager::GetInstance()->GetRootSignature("CubeMap"));
		cmd->SetPipelineState(YPipelineManager::GetInstance()->GetPipeLineStateObject("CubeMap"));
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto pm = YPipelineManager::GetInstance();
		const auto& indices = pm->GetParameterIndices("CubeMap");

		//-----------------------------------------
		// マテリアル & メッシュ描画コマンド
		//-----------------------------------------
		materialColor_->RecordDrawCommands(cmd, indices.at("gMaterialColor"));
		mesh_->RecordDrawCommands(cmd);

		//-----------------------------------------
		// 定数バッファ & テクスチャ設定
		//-----------------------------------------
		cmd->SetGraphicsRootConstantBufferView(indices.at("gTransformationMatrix"), wt_.GetConstBuffer()->GetGPUVirtualAddress());

		cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), TextureManager::GetInstance()->GetsrvHandleGPU(textureFilePath_));

		//-----------------------------------------
		// 描画実行
		//-----------------------------------------
		cmd->DrawIndexedInstanced(mesh_->GetIndexCount(), 1, 0, 0, 0);
	}

	/// <summary>
	/// テクスチャ変更（環境マップ再ロード）
	/// </summary>
	void CubeMap::SetTextureFilePath(const std::string& filePath)
	{
		textureFilePath_ = filePath;
		EnvironmentMap::GetInstance()->LoadEnvironmentTexture(textureFilePath_);
	}

	/// <summary>
	/// キューブマップ用のボックスメッシュ作成
	/// </summary>
	void CubeMap::CreateMesh()
	{
		mesh_ = MeshPrimitive::CreateBox(250.0f, 250.0f, 250.0f);
	}
}