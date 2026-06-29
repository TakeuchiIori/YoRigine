#pragma once

// C++
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>


// Math
#include "Matrix4x4.h"
#include "Vector3.h"


// モデルのマテリアルのライティング情報を扱うクラス
class MaterialLighting
{
public:
	///************************* GPU用の構造体 *************************///

	// マテリアルのライティング情報
	// ※ HLSL 側 (Object3d.PS / Object3dInstanced.PS) の MaterialLight と
	//    フィールド順・オフセットを必ず一致させること
	struct MaterialLight {
		int32_t enableLighting;     // 32ビット整数
		int32_t enableSpecular;     // 32ビット整数に変更
		int32_t enableEnvironment;  // 32ビット整数に変更
		int32_t isHalfVector;       // 32ビット整数に変更
		float shininess;            // 32ビット浮動小数点
		float environmentCoefficient; // 32ビット浮動小数点
		// --- トゥーン（セルシェーディング）拡張 ---
		int32_t enableToon;         // セルシェーディング有効
		int32_t toonBands;          // 階調数 (2〜)
		int32_t enableRim;          // リムライト有効
		int32_t enableToonSpecular; // トゥーンスペキュラ有効
		float   rimPower;           // リムの鋭さ (大きいほど縁が細い)
		float   rimIntensity;       // リムの強さ
		float   toonSpecularThreshold; // スペキュラを点灯するしきい値
		float   toonEdgeSoftness;   // バンド境界の smoothstep 幅 (0=完全ハード)
		// --- 2D表現（アニメ塗り）拡張 ---
		float   shadowStrength;     // 影色の強さ (0=影なし/フラット, 1=shadowColor全適用)
		float   _pad0;
		Vector3 shadowColor;        // 影部分の色（暗くする代わりにこの色へ寄せる）
		float   _pad1;
		Vector3 highlightColor;     // 固定ハイライトの色
		float   _pad2;              // 16バイトアライメント (合計 96 バイト)
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

	void SetEnableToon(bool enable) { materialLight_->enableToon = enable ? 1 : 0; }
	void SetToonBands(int bands) { materialLight_->toonBands = bands; }
	void SetEnableRim(bool enable) { materialLight_->enableRim = enable ? 1 : 0; }
	void SetEnableToonSpecular(bool enable) { materialLight_->enableToonSpecular = enable ? 1 : 0; }
	void SetRimPower(float value) { materialLight_->rimPower = value; }
	void SetRimIntensity(float value) { materialLight_->rimIntensity = value; }
	void SetToonSpecularThreshold(float value) { materialLight_->toonSpecularThreshold = value; }
	void SetToonEdgeSoftness(float value) { materialLight_->toonEdgeSoftness = value; }
	void SetShadowStrength(float value) { materialLight_->shadowStrength = value; }
	void SetShadowColor(const Vector3& c) { materialLight_->shadowColor = c; }
	void SetHighlightColor(const Vector3& c) { materialLight_->highlightColor = c; }

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

