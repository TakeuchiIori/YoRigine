#pragma once
// C++
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory>


// Math
#include "Vector4.h"


// マテリアルの色情報を管理するクラス
class MaterialColor
{
public:
	///************************* GPU用の構造体 *************************///

	// 色データ構造体
	struct ColorData {

		Vector4 color;

	};

	///************************* 基本的な関数 *************************///

	// 初期化
	void Initialize();

	// コマンドリストを積む
	void RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT index);

public:
	///************************* アクセッサ *************************///
	void SetColor(const Vector4& color) { colorData_->color = color; }
	void SetAlpha(float alpha) { colorData_->color.w = alpha; }
	const Vector4& GetColor() const { return colorData_->color; }
	Vector4& GetColor() { return colorData_->color; }

private:
	///************************* メンバ変数 *************************///

	// リソース関連
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	ColorData* colorData_ = nullptr;


};

