#pragma once

// C++
#include <d3d12.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

// Math
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

// assimp
#include <assimp/material.h>

namespace YoRigine {
class DirectXCommon;
}

// マテリアルクラス
class Material {
public:
  ///************************* GPU用の構造体 *************************///

  // melデータ
  struct MtlData {
    std::string name;
    float Ns;
    Vector3 Ka; // 環境光色
    Vector3 Kd; // 拡散反射色
    Vector3 Ks; // 鏡面反射光
    float Ni;
    float d;
    uint32_t illum;
    std::string textureFilePath;
    uint32_t textureIndex = 0;
  };

  // マテリアル定数用構造体
  // ※ HLSL 側 (Object3d.PS / Object3dInstanced.PS) の MaterialConstant と
  //    フィールド順・オフセットを必ず一致させること
  struct MaterialConstant {
    Vector3 Kd; // ベースカラー係数
    float padding[1];
  };

public:
  ///************************* 基本関数 *************************///

  // 初期化
  void Initialize(std::string &textureFilePath);

  // コマンドリストを積む。overrideTexturePath
  // を指定するとマテリアル本来のテクスチャの代わりに
  // そのテクスチャ（パスがキー）をバインドする（空なら自分のテクスチャを使う）。
  // overrideConstantCB に 0 以外を渡すと、マテリアル本来の定数バッファの
  // 代わりにそのアドレスをバインドする（オブジェクト個別のマテリアル上書き用）。
  void RecordDrawCommands(ID3D12GraphicsCommandList *command,
                          UINT rootParameterIndexCBV,
                          UINT rootParameterIndexSRV,
                          const std::string &overrideTexturePath = "",
                          D3D12_GPU_VIRTUAL_ADDRESS overrideConstantCB = 0);

  // mtlData_ の現在値から GPU 定数バッファを作り直す（エディタ編集後に呼ぶ）。
  void UploadConstants();

  // mtlData_ の現在値を GPU レイアウトに詰めて返す。
  // オブジェクト個別の上書き値とマージする際のベースとして使う。
  MaterialConstant BuildConstant() const;

private:
  // テクスチャ読み込み
  void LoadTexture();

public:
  ///************************* アクセッサ *************************///

  // マテリアルリソース
  ID3D12Resource *GetMaterialResource() { return materialResource_.Get(); }

  // マテリアルデータのアクセッサ
  const std::string &GetName() const { return mtlData_.name; }
  void SetName(const std::string &name) { mtlData_.name = name; }

  float GetNs() const { return mtlData_.Ns; }
  void SetNs(float ns) { mtlData_.Ns = ns; }

  const Vector3 &GetKa() const { return mtlData_.Ka; }
  void SetKa(const Vector3 &ka) { mtlData_.Ka = ka; }

  const Vector3 &GetKd() const { return mtlData_.Kd; }
  void SetKd(const Vector3 &kd) { mtlData_.Kd = kd; }

  const Vector3 &GetKs() const { return mtlData_.Ks; }
  void SetKs(const Vector3 &ks) { mtlData_.Ks = ks; }

  float GetNi() const { return mtlData_.Ni; }
  void SetNi(float ni) { mtlData_.Ni = ni; }

  float GetD() const { return mtlData_.d; }
  void SetD(float d) { mtlData_.d = d; }

  uint32_t GetIllum() const { return mtlData_.illum; }
  void SetIllum(uint32_t illum) { mtlData_.illum = illum; }

  // テクスチャファイルパス
  const std::string &GetTextureFilePath() const {
    return mtlData_.textureFilePath;
  }
  void SetTextureFilePath(const std::string &path) {
    mtlData_.textureFilePath = path;
  }

  // テクスチャインデックス
  uint32_t GetTextureIndex() const { return mtlData_.textureIndex; }
  void SetTextureIndex(uint32_t index) { mtlData_.textureIndex = index; }

private:
  ///************************* メンバ変数 *************************///
  YoRigine::DirectXCommon *dxCommon_ = nullptr;

  // リソース関連
  Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
  MtlData mtlData_;
  std::string textureFilePath_;
  Microsoft::WRL::ComPtr<ID3D12Resource> materialConstantResource_;
  MaterialConstant *materialConstant_ = nullptr;
};
