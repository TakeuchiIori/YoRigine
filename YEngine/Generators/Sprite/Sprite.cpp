#include "Sprite.h"
#include "SpriteCommon.h"
#include "Loaders./Texture./TextureManager.h"
#include "PipelineManager/YPipelineManager.h"

/// <summary>
/// コンストラクタ
/// </summary>
Sprite::Sprite()
{
}

/// <summary>
/// スプライトの初期化（テクスチャ読み込み・各種バッファ生成）
/// </summary>
/// <param name="textureFilePath">使用するテクスチャファイルパス</param>
void Sprite::Initialize(const std::string& textureFilePath)
{
	this->spriteCommon_ = SpriteCommon::GetInstance();

	srvManager_ = YoRigine::SrvManager::GetInstance();

	filePath_ = textureFilePath;

	// マテリアル定数バッファ生成
	MaterialResource();

	// テクスチャ読み込み
	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	// SRVインデックス取得
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// 頂点・インデックスバッファ生成
	VertexResource();
	IndexResource();

	// メタデータから画像サイズ取得
	AdjustTextureSize();

	// 初期Transform設定
	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
}

/// <summary>
/// スプライトの更新（頂点生成・行列更新）
/// </summary>
void Sprite::Update()
{
	/// 頂点更新
	CreateVertex();

	//────────────────────────────────
	// 3D → 2D 座標変換（camera_ がある場合のみ）
	//────────────────────────────────
	Vector3 screenPos = translate_; // デフォルトはそのまま使う

	if (camera_) {
		// 3D ワールド座標として扱う
		Vector4 worldPos = { translate_.x, translate_.y, translate_.z, 1.0f };

		// clip space = world * VP
		Vector4 clip = Transform(worldPos, camera_->GetViewProjectionMatrix());

		// 透視除算
		clip.x /= clip.w;
		clip.y /= clip.w;

		// NDC → Screen
		screenPos.x = (clip.x * 0.5f + 0.5f) * WinApp::kClientWidth;
		screenPos.y = (-clip.y * 0.5f + 0.5f) * WinApp::kClientHeight;
		screenPos.z = 0.0f; // スプライトは2D
	}

	//────────────────────────────────
	//スプライトの 2D 用 Transform を適用
	//────────────────────────────────
	transform_.translate = screenPos;
	transform_.rotate = rotate_;
	// 最終的な表示サイズ = 基準サイズ(px) × 拡縮倍率
	transform_.scale = { size_.x * scale_.x, size_.y * scale_.y, 1.0f };

	// 正射影行列で処理
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(
		0.0f, 0.0f,
		float(WinApp::kClientWidth),
		float(WinApp::kClientHeight),
		0.0f, 100.0f
	);

	transformationMatrixData_->WVP = Multiply(worldMatrix, projectionMatrix);
	transformationMatrixData_->World = worldMatrix;

	//────────────────────────────────
	// UV Transform
	//────────────────────────────────
	Matrix4x4 uvScaleMatrix = MakeScaleMatrix({ uvScale_.x, uvScale_.y, 1.0f });
	Matrix4x4 uvRotateMatrix = MakeRotateMatrixZ(uvRotation_);
	Matrix4x4 uvTranslateMatrix = MakeTranslateMatrix({ uvTranslation_.x, uvTranslation_.y, 0.0f });

	materialData_->uvTransform = Multiply(uvScaleMatrix, Multiply(uvRotateMatrix, uvTranslateMatrix));
}

/// <summary>
/// スプライト描画
/// </summary>
void Sprite::Draw()
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Sprite");


	// VBV設定
	spriteCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// IBV設定
	spriteCommon_->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);

	// マテリアルCB設定
	spriteCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(indices.at("gMaterial"), materialResource_->GetGPUVirtualAddress());

	// TransformCB設定
	spriteCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(indices.at("gTransformationMatrix"), transformationMatrixResource_->GetGPUVirtualAddress());

	// SRV設定
	srvManager_->SetGraphicsRootDescriptorTable(indices.at("gTexture"), textureIndex_);

	// 描画コール
	spriteCommon_->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(static_cast<UINT>(numVertices_), 1, 0, 0, 0);
}

/// <summary>
/// 頂点バッファリソース生成
/// </summary>
void Sprite::VertexResource()
{
	vertexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * static_cast<size_t>(numVertices_));

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(numVertices_);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

/// <summary>
/// 頂点データ生成（アンカー / 反転 / UV計算）
/// </summary>
void Sprite::CreateVertex()
{
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	/*=====================================================//
							 index有
	=======================================================*/

	// アンカーポイント補正（0.0～1.0）
	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	// 左右反転
	if (isFlipX_) {
		left = -left;
		right = -right;
	}
	// 上下反転
	if (isFlipY_) {
		top = -top;
		bottom = -bottom;
	}

	// テクスチャメタデータ取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);
	float tex_left = textureLeftTop_.x / metadata.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / metadata.width;
	float tex_top = textureLeftTop_.y / metadata.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metadata.height;

	// 左下
	vertexData[0].position = { left, bottom, 0.0f, 1.0f };
	vertexData[0].texcoord = { tex_left, tex_bottom };

	// 左上
	vertexData[1].position = { left, top, 0.0f, 1.0f };
	vertexData[1].texcoord = { tex_left, tex_top };

	// 右下
	vertexData[2].position = { right, bottom, 0.0f, 1.0f };
	vertexData[2].texcoord = { tex_right, tex_bottom };

	// 右上
	vertexData[3].position = { right, top, 0.0f, 1.0f };
	vertexData[3].texcoord = { tex_right, tex_top };

	// 2枚目用三角形
	vertexData[4].position = { left, top, 0.0f, 1.0f };
	vertexData[4].texcoord = { tex_left, tex_top };

	vertexData[5].position = { right, bottom, 0.0f, 1.0f };
	vertexData[5].texcoord = { tex_right, tex_bottom };

	// 法線
	vertexData[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };
	vertexData[4].normal = { 0.0f,0.0f,-1.0f };
	vertexData[5].normal = { 0.0f,0.0f,-1.0f };
}

/// <summary>
/// インデックスバッファ生成
/// </summary>
void Sprite::IndexResource()
{
	indexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * static_cast<size_t>(numVertices_));

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * static_cast<UINT>(numVertices_);
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	CreateIndex();
}

/// <summary>
/// インデックスデータ作成（三角形2枚）
/// </summary>
void Sprite::CreateIndex()
{
	uint32_t* indexData = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

	indexData[0] = 0;  // 1枚目
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 1;  // 2枚目
	indexData[4] = 3;
	indexData[5] = 2;
}

/// <summary>
/// マテリアルCB生成
/// </summary>
void Sprite::MaterialResource()
{
	materialResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f,1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();

	TransformResource();
}

/// <summary>
/// Transform行列CB生成
/// </summary>
void Sprite::TransformResource()
{
	transformationMatrixResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
}

/// <summary>
/// テクスチャのメタ情報からサイズを取得し、スプライトサイズに反映
/// </summary>
void Sprite::AdjustTextureSize()
{
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);

	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);

	size_ = textureSize_;
}

/// <summary>
/// スプライトのテクスチャを変更
/// </summary>
void Sprite::ChangeTexture(const std::string textureFilePath)
{
	filePath_ = textureFilePath;

	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	AdjustTextureSize();
}