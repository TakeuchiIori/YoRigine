#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>


// Math
#include "Matrix4x4.h"


// UV情報
class MaterialUV
{
public:
	///************************* GPU用の構造体 *************************///
	// HLSL 側 Object3d.PS.hlsl の Material 構造体と一致させること
	struct MaterialUVData {
		Matrix4x4 uvTransform;
		float     stochasticStrength; // 0 = 通常タイル / 1 = タイル単位ランダムを最大適用
		float     _pad[3];            // 16 byte アライメント
	};

	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// コマンドリストを積む
	void RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV);
public:
	///************************* アクセッサ *************************///
	void SetUVTransform(const Matrix4x4& uvTransform) { materialUV_->uvTransform = uvTransform; }
	void SetStochasticStrength(float s) { materialUV_->stochasticStrength = s; }

private:

	///************************* メンバ変数 *************************///
	// リソース関連
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	MaterialUVData* materialUV_ = nullptr;
};

