#pragma once
// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>

// Engine
#include "Systems/Camera/Camera.h"
#include "SrvManager./SrvManager.h"

// Math
#include "Vector4.h"
#include "Matrix4x4.h"
#include "MathFunc.h"
#include "Vector2.h"

#include "DirectXTex.h"
class SpriteCommon;
class Sprite
{
public: // 構造体
	// 頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// マテリアルデータ
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	// 座標変換データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

public: // 基本的関数
	Sprite();
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const std::string& textureFilePath);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// テクスチャ変更
	/// </summary>
	void ChangeTexture(const std::string textureFilePath);

private: // メンバ関数

	/// <summary>
	/// 頂点リソース
	/// </summary>
	void VertexResource();

	/// <summary>
	/// 頂点
	/// </summary>
	void CreateVertex();

	/// <summary>
	/// インデックスリソース
	/// </summary>
	void IndexResource();

	/// <summary>
	/// 頂点インデックス
	/// </summary>
	void CreateIndex();

	/// <summary>
	/// マテリアルリソース
	/// </summary>
	void MaterialResource();

	/// <summary>
	/// 座標変換リソース
	/// </summary>
	void TransformResource();

	/// <summary>
	/// テクスチャサイズをイメージに合わせる
	/// </summary>
	void AdjustTaxtureSize();


public: // アクセッサ

	/*===============================================//
						  座標
	//===============================================*/
	const Vector3& GetPosition()const { return position_; }
	void SetPosition(const Vector3& position) { position_ = position; }

	/*===============================================//
						  回転
	//===============================================*/
	Vector3 GetRotation()const { return rotation_; }
	void SetRotation(Vector3 rotation) { rotation_ = rotation; }

	/*===============================================//
						  拡縮
	//===============================================*/
	const Vector2& GetSize() { return size_; }
	void SetSize(const Vector2& size) { size_ = size; }

	/*===============================================//
					　	 色を変更
	//===============================================*/
	const Vector4& GetColor()const { return materialData_->color; }
	void SetColor(const Vector4& color) { materialData_->color = color; }

	/*===============================================//
				　		アルファ値の変更
	//===============================================*/

	void SetAlpha(const float& alpha) { materialData_->color.w = alpha; }

	/*===============================================//
					　アンカーポイント
	//===============================================*/
	// getter
	const Vector2& GetAnchorPoint()const { return anchorPoint_; }
	// setter
	void SetAnchorPoint(const Vector2& anchorPoint) { this->anchorPoint_ = anchorPoint; }

	/*===============================================//
					　    フリップ
	//===============================================*/
	// getter
	const bool& GetIsFlipX()const { return isFlipX_; }
	const bool& GetIsFlipY()const { return isFlipY_; }
	// setter
	void SetIsFlipX(const bool& isFlipX) { this->isFlipX_ = isFlipX; }
	void SetIsFlipY(const bool& isFlipY) { this->isFlipY_ = isFlipY; }

	/*===============================================//
					　テクスチャ範囲指定
	//===============================================*/
	// getter
	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
	const Vector2& GetTextureSize() const { return textureSize_; }
	// setter
	void SetTextureLeftTop(const Vector2& textureLeftTop) { this->textureLeftTop_ = textureLeftTop; }
	void SetTextureSize(const Vector2& textureSize) { this->textureSize_ = textureSize; }
	// SrvManagerのセッター
	void SetSrvManager(SrvManager* srvManager) { this->srvManagaer_ = srvManager; }

	void SetCamera(Camera* camera) { this->camera_ = camera; }

private: // メンバ変数

	SpriteCommon* spriteCommon_ = nullptr;
	SrvManager* srvManagaer_;
	Camera* camera_;
	/*===============================================//
						Resouurces
	//===============================================*/

	// 頂点
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// インデックス
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	// マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	// 座標変換
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	/*===============================================//
						Texture
	//===============================================*/

	DirectX::ScratchImage mipImages[2] = {};
	//const DirectX::TexMetadata& metadata = mipImages[0].GetMetadata();
	const DirectX::TexMetadata& metadata2 = mipImages[1].GetMetadata();

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource[2];
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource[2];

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
	// テクスチャ番号
	uint32_t textureIndex_ = 0;
	std::string filePath_;

	// テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f,0.0f };
	// テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 100.0f,100.0f };

	// スプライト
	Vector3 position_ = { 0.0f,0.0f ,0.0f};
	Vector3 rotation_ = { 0.0f,0.0f,0.0f };
	Vector2 size_ = { 100.0f,100.0f };

	// アンカーポイント
	Vector2 anchorPoint_ = { 0.0f,0.0f };

	// 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;

	EulerTransform transform_;
};

