#pragma once
// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>

// Engine
#include "Systems/Camera/Camera.h"
#include "SrvManager.h"

// Math
#include "Vector4.h"
#include "Matrix4x4.h"
#include "MathFunc.h"
#include "Vector2.h"

#include "DirectXTex.h"
class SpriteCommon;

/// <summary>
/// スプライトの生成クラス
/// </summary>
class Sprite
{
public:
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

public:
	///************************* 基本関数 *************************///
	Sprite();
	void Initialize(const std::string& textureFilePath);
	void Update();
	void Draw();

	// テクスチャ変更
	void ChangeTexture(const std::string textureFilePath);
	// テクスチャサイズをイメージに合わせる
	void AdjustTextureSize();


private:
	///************************* 内部処理 *************************///
	// 頂点の生成
	void CreateVertex();
	// 頂点リソース
	void VertexResource();
	// インデックスリソース
	void IndexResource();
	// 頂点インデックス
	void CreateIndex();
	// マテリアルリソース
	void MaterialResource();
	// 座標変換リソース
	void TransformResource();




public:
	///************************* アクセッサ *************************///
	/*===============================================//
						  座標
	//===============================================*/
	const Vector3& GetTranslate()const { return translate_; }
	void SetTranslate(const Vector3& translate) { translate_ = translate; }

	/*===============================================//
						  回転
	//===============================================*/
	Vector3 GetRotate()const { return rotate_; }
	void SetRotate(Vector3 rotate) { rotate_ = rotate; }

	/*===============================================//
						  拡縮
	//===============================================*/
	const Vector2& GetSize() { return size_; }
	void SetSize(const Vector2& scale) { size_ = scale; }

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
	const Vector2& GetAnchorPoint()const { return anchorPoint_; }
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
	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
	const Vector2& GetTextureSize() const { return textureSize_; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { this->textureLeftTop_ = textureLeftTop; }
	void SetTextureSize(const Vector2& textureSize) { this->textureSize_ = textureSize; }

	/*===============================================//
				　		UV SRT
	//===============================================*/
	const Vector2& GetUVTranslation() const { return uvTranslation_; }
	void SetUVTranslation(const Vector2& translation) { uvTranslation_ = translation; }

	float GetUVRotation() const { return uvRotation_; }
	void SetUVRotation(float rotation) { uvRotation_ = rotation; }

	const Vector2& GetUVScale() const { return uvScale_; }
	void SetUVScale(const Vector2& scale) { uvScale_ = scale; }

	/*===============================================//
				　		他クラスをセット
	//===============================================*/
	void SetSrvManager(SrvManager* srvManager) { this->srvManager_ = srvManager; }
	void SetCamera(Camera* camera) { this->camera_ = camera; }

private:
	///************************* メンバ変数 *************************///
	SpriteCommon* spriteCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;

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


	DirectX::ScratchImage mipImages[2] = {};
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
	Vector3 translate_ = { 0.0f,0.0f ,0.0f };
	Vector3 rotate_ = { 0.0f,0.0f,0.0f };
	Vector2 size_ = { 100.0f,100.0f };
	const float numVertices_ = 6.0f;

	// アンカーポイント
	Vector2 anchorPoint_ = { 0.0f,0.0f };

	// 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;
	EulerTransform transform_;

	// UV SRT
	Vector2 uvTranslation_ = { 0.0f, 0.0f };
	float uvRotation_ = 0.0f;
	Vector2 uvScale_ = { 1.0f, 1.0f };
};