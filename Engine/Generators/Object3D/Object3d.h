#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>

// Engine
#include "Systems/Camera/Camera.h"
#include "Loaders/Model/Model.h"
#include "../Graphics/Culling/OcclusionCullingManager.h"

// Math
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"


class Line;
class WorldTransform;
class Object3dCommon;
class Model;
// 3Dオブジェクト
class Object3d
{
public: // メンバ関数

	Object3d();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// アニメーションの更新
	/// </summary>
	void UpdateAnimation();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(Camera* camera,WorldTransform& worldTransform);

	/// <summary>
	/// スケルトン描画
	/// </summary>
	void DrawSkeleton(Line& line);

	/// <summary>
	/// モデルのセット
	/// </summary>
	void SetModel(const std::string& filePath, bool isAnimation = false);

	/// <summary>
	/// ImGui
	/// </summary>
	void MaterialByImGui();

private:

	/// <summary>
	/// マテリアルリソース作成
	/// </summary>
	void CreateMaterialResource();

	/// <summary>
	/// マテリアルリソース作成
	/// </summary>
	void CreateCameraResource();

public: // アクセッサ
	Model* GetModel() { return model_; }

	/*===============================================//
					　アンカーポイント
	//===============================================*/
	const Vector3& GetAnchorPoint()const { return anchorPoint_; }
	void SetAnchorPoint(const Vector3& anchorPoint) { this->anchorPoint_ = anchorPoint; }

	/*===============================================//
					　    フリップ
	//===============================================*/
	const bool& GetIsFlipX()const { return isFlipX_; }
	const bool& GetIsFlipY()const { return isFlipY_; }
	void SetIsFlipX(const bool& isFlipX) { this->isFlipX_ = isFlipX; }
	void SetIsFlipY(const bool& isFlipY) { this->isFlipY_ = isFlipY; }

	/*===============================================//
				　   	  カメラ
	//===============================================*/

	
	//void SetCamera(Camera* camera) { this->camera_ = camera; }
	//void SetLine(Line* line) { this->line_ = line; }

	// マテリアル
	const Vector4& GetMaterialColor() const { return materialData_->color; }
	void SetMaterialColor(const Vector4& color) { materialData_->color = color; }
	void SetAlpha(const float& alpha) { materialData_->color.w = alpha; }
	bool IsLightingEnabled() const { return materialData_->enableLighting != 0; }
	bool IsSpecularEnabled() const { return materialData_->enableSpecular; }
	bool IsHalfVectorEnabled() const { return materialData_->isHalfVector; }
	
	void SetLightingEnabled(bool enabled) { materialData_->enableLighting = enabled ? 1 : 0; }
	float GetMaterialShininess() const { return materialData_->shininess; }
	void SetMaterialShininess(float shininess) { materialData_->shininess = shininess; }
	const Matrix4x4& GetMaterialUVTransform() const { return materialData_->uvTransform; }
	void SetMaterialUVTransform(const Matrix4x4& uvTransform) { materialData_->uvTransform = uvTransform; }
	bool IsMaterialEnabled() const { return materialData_->enableLighting != 0; }
	void SetMaterialEnabled(bool enable) { materialData_->enableLighting = enable; }
	void SetMaterialSpecularEnabled(bool enable) { materialData_->enableSpecular = enable; }
	void SetMaterialHalfVectorEnabled(bool enable) { materialData_->isHalfVector = enable; }
private:
	// マテリアルデータ
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
		bool enableSpecular;
		bool isHalfVector;
	};

	struct CameraForGPU {
		Vector3 worldPosition;
		//float padding[3];
	};
	// マテリアルのリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	// 外部からのポインタ
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	//Camera* camera_ = nullptr;

	// テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f,0.0f };
	// テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 64.0f,64.0f };

	// アンカーポイント
	Vector3 anchorPoint_ = { 0.5f,0.5f,0.5f };
	// 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;

	// クエリインデックス
	UINT queryIndex_ = 0;
};

