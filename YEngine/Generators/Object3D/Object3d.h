#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>

// Engine
#include "Systems/Camera/Camera.h"
#include "Model.h"
#include "Material/MaterialColor.h"
#include "Material/MaterialLighting.h"
#include "Material/MaterialUV.h"
#include "Material/MaterialDissolve.h"
#include "Motion/Core/MotionSystem.h"
#include <Loaders/Json/JsonManager.h>

// Math
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"


class Line;
class WorldTransform;
class Object3dCommon;
class Model;

/// <summary>
/// オブジェクト生成クラス
/// </summary>
class Object3d
{
public:
	///************************* GPU用構造体 *************************///

	// シャドウマップ
	struct ObjectTransform
	{
		Matrix4x4 world;
	};

	///************************* Static関数 *************************///

	static std::unique_ptr<Object3d> Create(const std::string& filePath, const std::string& animationName = "", bool isAnimation = false);


	///************************* 基本的な関数 *************************///

	Object3d();

	// 初期化
	void Initialize();
	// アニメーションの更新
	void UpdateAnimation();
	// 描画
	void Draw(Camera* camera, WorldTransform& worldTransform);
	// スケルトン描画
	void DrawBone(Line& line, const Matrix4x4& worldMatrix);
	// 影描画
	void DrawShadow(WorldTransform& worldTransform);
	// モデルのセット
	void SetModel(const std::string& filePath, bool isAnimation = false, const std::string& animationName = "");
	// デバッグ表示
	void DebugInfo();

	///************************* モーション関連 *************************///
	// アニメーションを切り替える
	void SetChangeMotion(const std::string& filePath, MotionPlayMode playMode, const std::string& animationName = "");

	// 上半身のアニメーションを再生する
	void PlayUpperMotion(const std::string& filePath, MotionPlayMode playMode, const std::string& animationName = "");

	// 今のアニメーション速度を切り替え
	void SetMotionSpeed(float speed);
	// 上半身アニメーション速度を独立して切り替え
	void SetUpperMotionSpeed(float speed);
	// モーションの再生方法
	void PlayOnce() { model_->PlayOnce(); }
	void PlayLoop() { model_->PlayLoop(); }
	void Stop() { model_->Stop(); }
	void Resume() { model_->Resume(); }

public:
	// UVのSRT
	Vector2 uvScale = { 1.0f,1.0f };
	Vector2 uvTranslate = { 0.0f,0.0f };
	float uvRotate = 0.0f;
private:
	///************************* 内部処理*************************///

	// シャドウマップ用リソース作成
	void CreateShadowResources();

	// UVの更新
	void UpdateUV();

public:
	///************************* アクセッサ *************************///

	Model* GetModel() { return model_; }
	MaterialLighting* GetMaterialLighting() const { return materialLighting_.get(); }

	Vector4& GetColor() { return materialColor_->GetColor(); }
	void SetMaterialColor(const Vector4& color) { materialColor_->SetColor(color); }
	void SetAlpha(float alpha) { materialColor_->SetAlpha(alpha); }
	void SetUvTransform(const Matrix4x4& uvTransform) { materialUV_->SetUVTransform(uvTransform); }

	///************************* ディゾルブ *************************///
	MaterialDissolve* GetMaterialDissolve() const { return materialDissolve_.get(); }
	void  SetDissolveEnabled(bool enable) { materialDissolve_->SetEnabled(enable); }
	void  SetDissolveThreshold(float t) { materialDissolve_->SetThreshold(t); }
	void  SetDissolveEdgeWidth(float w) { materialDissolve_->SetEdgeWidth(w); }
	void  SetDissolveEdgeColor(const Vector3& c) { materialDissolve_->SetEdgeColor(c); }
	void  SetDissolveNoiseScale(float s) { materialDissolve_->SetNoiseScale(s); }
	bool  IsDissolveEnabled() const { return materialDissolve_->IsEnabled(); }
	float GetDissolveThreshold() const { return materialDissolve_->GetThreshold(); }

	void SetEnableLighting(bool enable) { materialLighting_->SetEnableLighting(enable); }
	void SetEnableSpecular(bool enable) { materialLighting_->SetEnableSpecular(enable); }
	void SetEnableEnvironment(bool enable) { materialLighting_->SetEnableEnvironment(enable); }
	void SetIsHalfVector(bool isHalf) { materialLighting_->SetIsHalfVector(isHalf); }
	void SetShininess(float shininess) { materialLighting_->SetShininess(shininess); }
	void SetEnvironmentCoefficient(float coeff) { materialLighting_->SetEnvironmentCoefficient(coeff); }

	bool IsLightingEnabled() const { return materialLighting_->IsLightingEnabled(); }
	bool IsSpecularEnabled() const { return materialLighting_->IsSpecularEnabled(); }
	bool IsEnvironmentEnabled() const { return materialLighting_->IsEnvironmentEnabled(); }
	bool IsHalfVector() const { return materialLighting_->IsHalfVector(); }
	float GetShininess() const { return materialLighting_->GetShininess(); }
	float GetEnvironmentCoefficient() const { return materialLighting_->GetEnvironmentCoefficient(); }

private:
	///************************* メンバ変数 *************************///


	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;

	// マテリアル関連
	std::unique_ptr<MaterialColor> materialColor_;
	std::unique_ptr<MaterialLighting> materialLighting_;
	std::unique_ptr<MaterialUV> materialUV_;
	std::unique_ptr<MaterialDissolve> materialDissolve_;

	// デフォルトのモデルパス
	static const std::string defaultModelPath_;
	// シャドウマップ用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> objectCB_;
	ObjectTransform* objectData_;
};

