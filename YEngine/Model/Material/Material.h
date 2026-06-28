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
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"

// assimp
#include <assimp/material.h>


namespace YoRigine {
	class DirectXCommon;
}

// マテリアルクラス
class Material
{
public:

	///************************* GPU用の構造体 *************************///

	// melデータ
	struct MtlData {
		std::string name;
		float Ns;
		Vector3 Ka;	// 環境光色
		Vector3 Kd;	// 拡散反射色
		Vector3 Ks;	// 鏡面反射光
		float Ni;
		float d;
		uint32_t illum;
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	// マテリアル定数用構造体
	struct MaterialConstant {
		Vector3 Kd;
		float padding[1];
	};


public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize(std::string& textureFilePath);

	// コマンドリストを積む
	void RecordDrawCommands(ID3D12GraphicsCommandList* command, UINT rootParameterIndexCBV, UINT rootParameterIndexSRV);

private:

	// テクスチャ読み込み
	void LoadTexture();
public:
	///************************* アクセッサ *************************///

	// マテリアルリソース
	ID3D12Resource* GetMaterialResource() { return materialResource_.Get(); }


	// マテリアルデータのアクセッサ
	const std::string& GetName() const { return mtlData_.name; }
	void SetName(const std::string& name) { mtlData_.name = name; }

	float GetNs() const { return mtlData_.Ns; }
	void SetNs(float ns) { mtlData_.Ns = ns; }

	const Vector3& GetKa() const { return mtlData_.Ka; }
	void SetKa(const Vector3& ka) { mtlData_.Ka = ka; }

	const Vector3& GetKd() const { return mtlData_.Kd; }
	void SetKd(const Vector3& kd) { mtlData_.Kd = kd; }

	const Vector3& GetKs() const { return mtlData_.Ks; }
	void SetKs(const Vector3& ks) { mtlData_.Ks = ks; }

	float GetNi() const { return mtlData_.Ni; }
	void SetNi(float ni) { mtlData_.Ni = ni; }

	float GetD() const { return mtlData_.d; }
	void SetD(float d) { mtlData_.d = d; }

	uint32_t GetIllum() const { return mtlData_.illum; }
	void SetIllum(uint32_t illum) { mtlData_.illum = illum; }


	// テクスチャファイルパス
	const std::string& GetTextureFilePath() const { return mtlData_.textureFilePath; }
	void SetTextureFilePath(const std::string& path) { mtlData_.textureFilePath = path; }

	// テクスチャインデックス
	uint32_t GetTextureIndex() const { return mtlData_.textureIndex; }
	void SetTextureIndex(uint32_t index) { mtlData_.textureIndex = index; }



private:
	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_ = nullptr;

	// リソース関連
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	MtlData mtlData_;
	std::string textureFilePath_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialConstantResource_;
	MaterialConstant* materialConstant_ = nullptr;

};

