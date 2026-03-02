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
	struct MaterialUVData {
		Matrix4x4 uvTransform;
		//float padding[3];
	};

	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// コマンドリストを積む
	void RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV);
public:
	///************************* アクセッサ *************************///
	void SetUVTransform(const Matrix4x4& uvTransform) { materialUV_->uvTransform = uvTransform; }

private:

	///************************* メンバ変数 *************************///
	// リソース関連
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	MaterialUVData* materialUV_ = nullptr;
};

