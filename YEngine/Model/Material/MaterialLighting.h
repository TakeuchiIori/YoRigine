#pragma once

// C++
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>


// Math
#include "Matrix4x4.h"


// モデルのマテリアルのライティング情報を扱うクラス
class MaterialLighting
{
public:
	///************************* GPU用の構造体 *************************///

	// マテリアルのライティング情報
	struct MaterialLight {
		int32_t enableLighting;     // 32ビット整数
		int32_t enableSpecular;     // 32ビット整数に変更
		int32_t enableEnvironment;  // 32ビット整数に変更
		int32_t isHalfVector;       // 32ビット整数に変更
		float shininess;            // 32ビット浮動小数点
		float environmentCoefficient; // 32ビット浮動小数点
		float padding[2];           // 16バイトアライメントのためのパディング
	};


	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// コマンドリストを積む
	void RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV);

public:
	///************************* アクセッサ *************************///

	MaterialLight* GetRaw() { return materialLight_; }


	void SetEnableLighting(bool enable) { materialLight_->enableLighting = enable ? 1 : 0; }
	void SetEnableSpecular(bool enable) { materialLight_->enableSpecular = enable ? 1 : 0; }
	void SetEnableEnvironment(bool enable) { materialLight_->enableEnvironment = enable ? 1 : 0; }
	void SetIsHalfVector(bool isHalf) { materialLight_->isHalfVector = isHalf ? 1 : 0; }
	void SetShininess(float value) { materialLight_->shininess = value; }
	void SetEnvironmentCoefficient(float value) { materialLight_->environmentCoefficient = value; }

	bool IsLightingEnabled() const { return materialLight_->enableLighting != 0; }
	bool IsSpecularEnabled() const { return materialLight_->enableSpecular != 0; }
	bool IsEnvironmentEnabled() const { return materialLight_->enableEnvironment != 0; }
	bool IsHalfVector() const { return materialLight_->isHalfVector != 0; }
	float GetShininess() const { return materialLight_->shininess; }
	float GetEnvironmentCoefficient() const { return materialLight_->environmentCoefficient; }

private:
	///************************* メンバ変数 *************************///

	// リソース関連
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	MaterialLight* materialLight_ = nullptr;

};

