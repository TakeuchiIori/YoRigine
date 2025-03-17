#include "Sprite.h"
#include "SpriteCommon.h"
#include "Loaders./Texture./TextureManager.h"

Sprite::Sprite()
{
}

void Sprite::Initialize(const std::string& textureFilePath)
{
	this->spriteCommon_ = SpriteCommon::GetInstance();

	srvManagaer_ = SrvManager::GetInstance();

	filePath_ = textureFilePath;

	MaterialResource();

	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	VertexResource();
	IndexResource();

	AdjustTaxtureSize();

	transform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	CreateVertex();
}

void Sprite::Update()
{
	
	
	transform_.translate = { position_.x,position_.y,position_.z};
	transform_.rotate = { rotation_.x,rotation_.y,rotation_.z };
	transform_.scale = { size_.x,size_.y,1.0f };
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	Matrix4x4 worldProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

	transformationMatrixData_->WVP = worldProjectionMatrix;
	transformationMatrixData_->World = worldMatrix;
	// WVP行列を更新
	if (camera_) {
		transformationMatrixData_->WVP = worldProjectionMatrix * camera_->GetViewProjectionMatrix();
	}
	else {
		transformationMatrixData_->WVP = worldProjectionMatrix;
	}
}

void Sprite::Draw()
{

	// VertexBufferView
	spriteCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_); // VBVを設定
	// IndexBufferView
	spriteCommon_->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);//IBV

	// マテリアルCBufferの場所を指定
	spriteCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// TransformatonMatrixCBuffferの場所を設定
	spriteCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

	// SRVの設定
	srvManagaer_->SetGraphicsRootDescriptorTable(2, textureIndex_);
	// 描画！！！DrawCall/ドローコール）
	spriteCommon_->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}


void Sprite::VertexResource()
{
	// リソース
	vertexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 6);
	// リソースの先頭アドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 使用するリソースサイズは頂点3つ分のサイズ
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	// 1頂点あたりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Sprite::CreateVertex()
{
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	/*=====================================================//
							 index有
	=======================================================*/

	// アンカーポイント
	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	// 左右反転
	if (isFlipX_) {
		left = -left;
		right = -right;
	}
	//上下反転
	if (isFlipY_) {
		top = -top;
		bottom = -bottom;
	}

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

	// 2枚目の三角形用
	vertexData[4].position = { left, top, 0.0f, 1.0f };  // 左上
	vertexData[4].texcoord = { tex_left, tex_top };

	vertexData[5].position = { right, bottom, 0.0f, 1.0f };  // 右下
	vertexData[5].texcoord = { tex_right, tex_bottom };

	vertexData[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };
	vertexData[4].normal = { 0.0f,0.0f,-1.0f };
	vertexData[5].normal = { 0.0f,0.0f,-1.0f };

	//// 1枚目の三角形
	//vertexData[0].position = { 0.0f,360.0f,0.0f,1.0f };   //左下
	//vertexData[0].texcoord = { 0.0f,1.0f };
	//vertexData[1].position = { 0.0f,0.0f,0.0f,1.0f };     //左上
	//vertexData[1].texcoord = { 0.0f,0.0f };
	//vertexData[2].position = { 640.0f,360.0f,0.0f,1.0f }; //右下
	//vertexData[2].texcoord = { 1.0f,1.0f };
	////2枚目の三角形
	//vertexData[3].position = { 640.0f,0.0f,0.0f,1.0f };   //右上
	//vertexData[3].texcoord = { 1.0f,0.0f };
	//vertexData[4].position = { 0.0f, 0.0f, 0.0f, 1.0f };  //左上
	//vertexData[4].texcoord = { 0.0f, 0.0f };
	//vertexData[5].position = { 640.0f,360.0f,0.0f,1.0f }; //右下
	//vertexData[5].texcoord = { 1.0f,1.0f };
	//// 書き込むためのアドレスを取得
	//vertexData[0].normal = { 0.0f,0.0f,-1.0f };
	
	//vertexResource_->Unmap(0, nullptr);
}

void Sprite::IndexResource()
{
	// リソース
	indexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);
	// リソースの先頭アドレスから使う
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	// 使用するリソースサイズはもとの頂点のサイズ
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	CreateIndex();
}

void Sprite::CreateIndex()
{
	uint32_t* indexData = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	indexData[0] = 0;  // 最初の三角形
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 1;  // 2つ目の三角形
	indexData[4] = 3;
	indexData[5] = 2;
	//indexResource_->Unmap(0, nullptr);
}

void Sprite::MaterialResource()
{
	// リソース作成
	materialResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
	// データを書き込むためのアドレスを取得して割り当て
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	// マテリアルデータの初期化
	materialData_->color = { 1.0f,1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();


	TransformResource();
}

void Sprite::TransformResource()
{
	// リソース作成
	transformationMatrixResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	// データを書き込むためのアドレスを取得して割り当て
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	// 単位行列を書き込む
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
}

void Sprite::AdjustTaxtureSize()
{
	// テクスチャメタデータを取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);

	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);
	// 画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}

void Sprite::ChangeTexture(const std::string textureFilePath)
{
	// 新しいテクスチャをロード
	TextureManager::GetInstance()->LoadTexture(textureFilePath);

	// 新しいテクスチャのインデックスを取得
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// SRVのハンドルを更新
	//textureSrvHandleGPU = TextureManager::GetInstance()->GetsrvHandleGPU(textureFilePath);
}


